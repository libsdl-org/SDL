#!/usr/bin/env python

import argparse
import contextlib
import logging
import os
import pathlib
import shlex
import sys
import time
from typing import Optional
import urllib.parse

from selenium import webdriver
import selenium.common.exceptions
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait


logger = logging.getLogger(__name__)


class SDLSeleniumTestDriver:
    def __init__(self, server: str, test: str, arguments: list[str], browser: str, firefox_binary: Optional[str]=None, chrome_binary: Optional[str]=None):
        self. server = server
        self.test = test
        self.arguments = arguments
        self.chrome_binary = chrome_binary
        self.firefox_binary = firefox_binary
        self.driver = None
        self.stdout_printed = False
        self.failed_messages: list[str] = []
        self.return_code = None

        driver_contructor = None
        match browser:
            case "firefox":
                driver_contructor = webdriver.Firefox
                driver_options = webdriver.FirefoxOptions()
                if self.firefox_binary:
                    driver_options.binary_location = self.firefox_binary
            case "chrome":
                driver_contructor = webdriver.Chrome
                driver_options = webdriver.ChromeOptions()
                if self.chrome_binary:
                    driver_options.binary_location = self.chrome_binary
        if driver_contructor is None:
            raise ValueError(f"Invalid {browser=}")

        options = [
            "--headless",
        ]
        for o in options:
            driver_options.add_argument(o)
        logger.debug("About to create driver")
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
            "SDL_ASSERT": "abort",
        }
        for key, value in os.environ.items():
            if key.startswith("SDL_"):
                req[key] = value
        req.update({f"arg_{i}": a for i, a in enumerate(self.arguments, 1) })
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
            self.failed_messages.append("Failed to get stdout/stderr")



def main() -> int:
    parser = argparse.ArgumentParser(allow_abbrev=False, description="Selenium SDL test driver")
    parser.add_argument("--browser", default="firefox", choices=["firefox", "chrome"], help="browser")
    parser.add_argument("--server", default="http://localhost:8080", help="Server where SDL tests live")
    parser.add_argument("--verbose", action="store_true", help="Verbose logging")
    parser.add_argument("--chrome-binary", help="Chrome binary")
    parser.add_argument("--firefox-binary", help="Firefox binary")

    index_double_dash = sys.argv.index("--")
    if index_double_dash < 0:
        parser.error("Missing test arguments. Need -- <FILENAME> <ARGUMENTS>")
    driver_arguments = sys.argv[1:index_double_dash]
    test = pathlib.Path(sys.argv[index_double_dash+1]).name
    test_arguments = sys.argv[index_double_dash+2:]

    args = parser.parse_args(args=driver_arguments)

    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)

    logger.debug("driver_arguments=%r test=%r test_arguments=%r", driver_arguments, test, test_arguments)

    sdl_test_driver = SDLSeleniumTestDriver(
        server=args.server,
        test=test,
        arguments=test_arguments,
        browser=args.browser,
        chrome_binary=args.chrome_binary,
        firefox_binary=args.firefox_binary,
    )
    sdl_test_driver.loop()

    rc = sdl_test_driver.return_code
    if sdl_test_driver.failed_messages:
        for msg in sdl_test_driver.failed_messages:
            print(f"FAILURE MESSAGE: {msg}", file=sys.stderr)
        if rc == 0:
            print(f"Test signaled success (rc=0) but a failure happened", file=sys.stderr)
            rc = 1
    sys.stdout.flush()
    logger.info("Exit code = %d", rc)
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
