# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

import collections
import contextlib
import datetime
import os
import shutil
import signal
import subprocess
import time
import typing

from ..logger import get_logger
from ..platform_helpers import is_windows, is_macOS
from ..run import run

_log = get_logger("util.android")


SdkToolPaths = collections.namedtuple("SdkToolPaths", ["emulator", "adb", "sdkmanager", "avdmanager"])


def get_sdk_tool_paths(sdk_root: str):
    def filename(name, windows_extension):
        if is_windows():
            return f"{name}.{windows_extension}"
        else:
            return name

    def resolve_path(dirnames, basename):
        dirnames.insert(0, "")
        for dirname in dirnames:
            path = shutil.which(os.path.join(dirname, basename))
            if path is not None:
                path = os.path.realpath(path)
                _log.debug(f"Found {basename} at {path}")
                return path
        raise FileNotFoundError(f"Failed to resolve path for {basename}")

    return SdkToolPaths(
        emulator=resolve_path([os.path.join(sdk_root, "emulator")], filename("emulator", "exe")),
        adb=resolve_path([os.path.join(sdk_root, "platform-tools")], filename("adb", "exe")),
        sdkmanager=resolve_path(
            [os.path.join(sdk_root, "cmdline-tools", "latest", "bin")],
            filename("sdkmanager", "bat"),
        ),
        avdmanager=resolve_path(
            [os.path.join(sdk_root, "cmdline-tools", "latest", "bin")],
            filename("avdmanager", "bat"),
        ),
    )


def create_virtual_device(sdk_tool_paths: SdkToolPaths, system_image_package_name: str, avd_name: str):
    run(sdk_tool_paths.sdkmanager, "--install", system_image_package_name, input=b"y")

    run(
        sdk_tool_paths.avdmanager,
        "create",
        "avd",
        "--name",
        avd_name,
        "--package",
        system_image_package_name,
        "--force",
        input=b"no",
    )


_process_creationflags = subprocess.CREATE_NEW_PROCESS_GROUP if is_windows() else 0


def _start_process(*args) -> subprocess.Popen:
    _log.debug(f"Starting process - args: {[*args]}")
    return subprocess.Popen([*args], creationflags=_process_creationflags)


_stop_signal = signal.CTRL_BREAK_EVENT if is_windows() else signal.SIGTERM


def _stop_process(proc: subprocess.Popen):
    _log.debug(f"Stopping process - args: {proc.args}")
    proc.send_signal(_stop_signal)

    try:
        proc.wait(30)
    except subprocess.TimeoutExpired:
        _log.warning("Timeout expired, forcibly stopping process...")
        proc.kill()


def _stop_process_with_pid(pid: int):
    # minimize scope of external module usage
    import psutil

    if psutil.pid_exists(pid):
        process = psutil.Process(pid)
        _log.debug(f"Stopping process - pid: {pid}")
        process.terminate()
        try:
            process.wait(60)
        except psutil.TimeoutExpired:
            print("Process did not terminate within 60 seconds. Killing.")
            process.kill()
            time.sleep(10)
            if psutil.pid_exists(pid):
                print(f"Process still exists. State:{process.status()}")


def start_emulator(
    sdk_tool_paths: SdkToolPaths, avd_name: str, extra_args: typing.Optional[typing.Sequence[str]] = None
) -> subprocess.Popen:
    with contextlib.ExitStack() as emulator_stack, contextlib.ExitStack() as waiter_stack:
        emulator_args = [
            sdk_tool_paths.emulator,
            "-avd",
            avd_name,
            "-memory",
            "4096",
            "-timezone",
            "America/Los_Angeles",
            "-no-snapstorage",
            "-no-audio",
            # allow a window so we can potentially do something like this in a CI to capture the desktop and the
            # emulator screen
            #   screencapture screenshot.jpg
            #   $(ANDROID_SDK_HOME)/platform-tools/adb exec-out screencap -p > emulator.png
            # and publish screenshot.jpg and emulator.png as artifacts
            ("" if is_macOS() else "-no-window"),
            "-no-boot-anim",
            "-gpu",
            "guest",
            "-delay-adb",
        ]
        if extra_args is not None:
            emulator_args += extra_args

        emulator_process = emulator_stack.enter_context(_start_process(*emulator_args))
        emulator_stack.callback(_stop_process, emulator_process)

        # we're specifying -delay-adb so if that works correctly as soon as an adb command completes we should be good
        waiter_process = waiter_stack.enter_context(
            _start_process(
                sdk_tool_paths.adb,
                "wait-for-device",
                "shell",
                # "while [[ -z $(getprop sys.boot_completed) | tr -d '\r' ]]; do echo 'waiting'; sleep 10; done; input keyevent 82",
                "ls /data/local/tmp",
            )
        )

        waiter_stack.callback(_stop_process, waiter_process)

        # poll subprocesses.
        # allow 20 minutes for startup as some CIs are slow. TODO: Make timeout configurable if needed.
        sleep_interval_seconds = 10
        end_time = datetime.datetime.now() + datetime.timedelta(minutes=20)

        while True:
            waiter_ret, emulator_ret = waiter_process.poll(), emulator_process.poll()

            if emulator_ret is not None:
                # emulator exited early
                raise RuntimeError(f"Emulator exited early with return code: {emulator_ret}")

            if waiter_ret is not None:
                if waiter_ret == 0:
                    break
                raise RuntimeError(f"Waiter process exited with return code: {waiter_ret}")

            if datetime.datetime.now() > end_time:
                raise RuntimeError("Emulator startup timeout")

            time.sleep(sleep_interval_seconds)

        # emulator is ready now but do one more check
        emulator_stack.pop_all()

        while True:
            getprop_output = subprocess.check_output(
                [sdk_tool_paths.adb,
                # "wait-for-device",
                "shell",
                # "while [[ -z $(getprop sys.boot_completed) | tr -d '\r' ]]; do echo 'waiting...'; sleep 5; done; input keyevent 82",
                "getprop sys.boot_completed"],
                timeout=10
            )

            if bytes.decode(getprop_output).strip() == '1':
                break

            elif datetime.datetime.now() > end_time:
                raise RuntimeError("Emulator startup timeout. sys.boot_completed was not set.")

            time.sleep(10)

        return emulator_process


def stop_emulator(emulator_proc_or_pid: typing.Union[subprocess.Popen, int]):
    if isinstance(emulator_proc_or_pid, subprocess.Popen):
        _stop_process(emulator_proc_or_pid)
    elif isinstance(emulator_proc_or_pid, int):
        _stop_process_with_pid(emulator_proc_or_pid)
    else:
        raise ValueError("Expected either a PID or subprocess.Popen instance.")
