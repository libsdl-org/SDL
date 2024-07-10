#!/usr/bin/env python

import argparse
import contextlib
import urllib.parse
import shlex
import sys
import time

from selenium import webdriver
import selenium.common.exceptions
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait


class SDLSeleniumTestDriver:
    def __init__(self, server, test, arguments, browser):
        self. server = server
        self.test = test
        self.arguments = arguments
        self.driver = None
        self.stdout_printed = False
        self.failed_messages = []
        self.return_code = None

        driver_contructor = None
        match browser:
            case "firefox":
                driver_contructor = webdriver.Firefox
                driver_options = webdriver.FirefoxOptions()
            case "chrome":
                driver_contructor = webdriver.Chrome
                driver_options = webdriver.ChromeOptions()
        if driver_contructor is None:
            raise ValueError(f"Invalid {browser=}")

        options = [
            "--headless",
        ]
        for o in options:
            driver_options.add_argument(o)
        self.driver = driver_contructor(options=driver_options)

    @property
    def finished(self):
        return len(self.failed_messages) > 0 or self.return_code is not None

    def __del__(self):
        if self.driver:
            self.driver.quit()

    @property
    def url(self):
        req = {
            "loghtml": "1",
            "onassert": "abort",
        }
        req.update({f"arg_{i}": a for i, a in enumerate(self.arguments) })
        req_str = urllib.parse.urlencode(req)
        return f"{self.server}/{self.test}.html?{req_str}"

    @contextlib.contextmanager
    def _selenium_catcher(self):
        try:
            yield
            success = True
        except selenium.common.exceptions.UnexpectedAlertPresentException as e:
            # FIXME: switch context, verify text of dialog and answer "a" for abort
            wait = WebDriverWait(self.driver, timeout=2)
            try:
                alert = wait.until(lambda d: d.switch_to.alert)
            except selenium.common.exceptions.NoAlertPresentException:
                self.failed_messages.append(e.msg)
                return False
            self.failed_messages.append(alert)
            if "Assertion failure" in e.msg and "[ariA]" in e.msg:
                alert.send_keys("a")
                alert.accept()
            else:
                self.failed_messages.append(e.msg)
            success = False
        return success

    def get_stdout_and_print(self):
        if self.stdout_printed:
            return
        with self._selenium_catcher():
            div_terminal = self.driver.find_element(by=By.ID, value="terminal")
            assert div_terminal
            text = div_terminal.text
            print(text)
            self.stdout_printed = True

    def update_return_code(self):
        with self._selenium_catcher():
            div_process_quit = self.driver.find_element(by=By.ID, value="process-quit")
            if not div_process_quit:
                return
            if div_process_quit.text != "":
                try:
                    self.return_code = int(div_process_quit.text)
                except ValueError:
                    raise ValueError(f"process-quit element contains invalid data: {div_process_quit.text:r}")

    def loop(self):
        print(f"Connecting to \"{self.url}\"", file=sys.stderr)
        self.driver.get(url=self.url)
        self.driver.implicitly_wait(0.2)

        while True:
            self.update_return_code()
            if self.finished:
                break
            time.sleep(0.1)

        self.get_stdout_and_print()
        if not self.stdout_printed:
            self.failed_messages.append("Failed to gest stdout/stderr")


def main():
    parser = argparse.ArgumentParser(allow_abbrev=False, description="Selenium SDL test driver")
    parser.add_argument("--browser", default="firefox", choices=["firefox", "chrome"], help="browser")
    parser.add_argument("--server", default="http://localhost:8080", help="Server where SDL tests live")
    parser.add_argument("--test", required=True, help="Name of test to run")
    parser.add_argument("--arguments", default=None, help="Test arguments, appended as one string")
    args = parser.parse_args()

    test_arguments = []
    if args.arguments:
        test_arguments = shlex.split(args.arguments)

    sdl_test_driver = SDLSeleniumTestDriver(
        server=args.server,
        test=args.test,
        arguments=test_arguments,
        browser=args.browser,
    )
    sdl_test_driver.loop()

    rc = sdl_test_driver.return_code
    if sdl_test_driver.failed_messages:
        for msg in sdl_test_driver.failed_messages:
            print(f"FAILURE MESSAGE: {msg}", file=sys.stderr)
        if rc == 0:
            print(f"Test signaled success (rc=0) but a failure happened", file=sys.stderr)
            rc = 1
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
