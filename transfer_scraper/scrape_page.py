import json
import html
import sys
import re
import os.path
from time import sleep
import random
from fake_useragent import UserAgent
from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
from selenium.common.exceptions import (
    StaleElementReferenceException,
    NoSuchElementException,
)


class IPBanException(Exception):
    pass


# Fix course titles accounting for Roman numerals up to X
def normalize_title(input):
    s = " ".join(input.split())
    s = re.sub(r"[A-Za-z]+(['‘’][A-Za-z]+)?", lambda m: m.group(0).capitalize(), s)
    s = re.sub(r"\b(Viii|Vii|Vi|Iv|Ix|Iii|Ii)\b", lambda a: a.group(0).upper(), s)
    return s.strip()


# Waits until EC plus some random wait time
def wait(ec):
    global driver

    WebDriverWait(
        driver, 35, ignored_exceptions=[StaleElementReferenceException]
    ).until(ec)
    sleep(random.uniform(400, 1900) / 1000)


# jump_to_page: navigates to a paginated page on this insufferable website
#
# curr_page: the current page number
# to_page: the page number to jump to
# num_pages: the total number of pages
# postback_type: javascript:__doPostBack('<this field>','Page$3')
# pagination_type: <span id="<this field>">PAGE 1 OF 27<br></span>
def jump_to_page(curr_page, to_page, postback_type, pagination_type):
    global driver

    wait(EC.visibility_of_element_located((By.ID, postback_type)))
    page = driver.find_element(By.ID, postback_type)
    try:
        num_pages = int(driver.find_element(By.ID, pagination_type).text.split()[-1])
    except NoSuchElementException:
        return 1, page

    if to_page > num_pages or to_page < 1:
        raise ValueError(
            f"to_page was out of range ({to_page} not in [1, {num_pages}])"
        )
    while curr_page != to_page:
        jumpable_pages = {
            int(x.get_attribute("href").split("'")[3][5:]): x
            for x in driver.find_elements(
                By.CSS_SELECTOR,
                """a[href^="javascript:__doPostBack('"""
                + postback_type
                + """','Page$"]""",
            )
        }
        curr_page = int(driver.find_element(By.ID, pagination_type).text.split()[-3])
        if to_page in jumpable_pages:
            jumpable_pages[to_page].click()
            curr_page = to_page
        elif to_page < min(jumpable_pages):
            jumpable_pages[min(jumpable_pages)].click()
            curr_page = min(jumpable_pages)
        else:
            jumpable_pages[max(jumpable_pages)].click()
            curr_page = max(jumpable_pages)

        wait(EC.staleness_of(page))
        sleep(random.uniform(400, 1900) / 1000)
        page = driver.find_element(By.ID, postback_type)
    return curr_page, page


# scrape_page: Scrapes a page of institutions
#
# page_num: The page to scrape.
# Note that the current page before running this function must be 1.
def scrape_page(page_num):
    global driver
    global options

    for i in range(1, 4):
        try:
            driver = webdriver.Firefox(options=options)
            driver.get("https://ipinfo.io/ip")
            print(
                f'Trying with IP {driver.find_element(By.TAG_NAME, "body").text}',
                file=sys.stderr,
            )
            driver.get(
                "https://tes.collegesource.com/publicview/TES_publicview01.aspx?rid=f080a477-bff8-46df-a5b2-25e9affdd4ed&aid=27b576bb-cd07-4e57-84d0-37475fde70ce"
            )
            wait(EC.visibility_of_element_located((By.TAG_NAME, "body")))
            print(f'Title: "{driver.title}"', file=sys.stderr)
            if driver.title == "403 Forbidden":
                raise IPBanException
            jump_to_page(1, page_num, "gdvInstWithEQ", "lblInstWithEQPaginationInfo")
            break
        except IPBanException as e:
            driver.quit()
            print(f"We are IP-banned, exiting now", file=sys.stderr)
            raise e
        except Exception as e:
            driver.quit()

            print(
                f"Attempt {i} failed due to {type(e).__name__}, retrying in 25 seconds...",
                file=sys.stderr,
            )
            sleep(25)
    else:
        raise Exception(f"Failed to load the main page after 4 attempts, aborting.")

    num_institutions = len(
        driver.find_elements(
            By.CSS_SELECTOR, "a[id^=gdvInstWithEQ_btnCreditFromInstName_]"
        )
    )
    driver.quit()

    print(f"Scraping page {page_num}, found {num_institutions} links", file=sys.stderr)
    return [scrape_institution_safe(i, page_num) for i in range(0, num_institutions)]


def scrape_institution_safe(index, page_num):
    for i in range(1, 4):
        try:
            return scrape_institution(index, page_num)
        except Exception as e:
            driver.quit()
            print(
                f"\tAttempt {i} failed due to {type(e).__name__}, retrying in 25 seconds...",
                file=sys.stderr,
            )
            sleep(25)
    else:
        raise Exception(f"Failed to scrape {index} after 4 attempts, aborting.")


# scrape_institution: Scrapes an institution by index.
#
# index: the 0-indexed index of the instituion to scrape on the page we are on.
def scrape_institution(index, page_num):
    global driver
    global options

    driver = webdriver.Firefox(options=options)
    driver.get(
        "https://tes.collegesource.com/publicview/TES_publicview01.aspx?rid=f080a477-bff8-46df-a5b2-25e9affdd4ed&aid=27b576bb-cd07-4e57-84d0-37475fde70ce"
    )
    jump_to_page(1, page_num, "gdvInstWithEQ", "lblInstWithEQPaginationInfo")

    inst_link = driver.find_element(
        By.ID, f"gdvInstWithEQ_btnCreditFromInstName_{index}"
    )
    [inst_name, inst_city, inst_state, _] = [
        e.text
        for e in inst_link.find_element(By.XPATH, "../..").find_elements(
            By.TAG_NAME, "td"
        )
    ]
    inst_name, inst_city = normalize_title(inst_name), normalize_title(inst_city)
    inst_link.click()
    wait(EC.staleness_of(inst_link))
    print(f"Scraping {inst_name} ({inst_city}, {inst_state})", file=sys.stderr)

    # Add all courses
    try:
        num_pages = int(
            driver.find_element(By.ID, "lblCourseEQPaginationInfo").text.split()[-1]
        )
    except NoSuchElementException:
        num_pages = 1

    try:
        for i in range(1, num_pages + 1):
            jump_to_page(max(1, i - 1), i, "gdvCourseEQ", "lblCourseEQPaginationInfo")
            driver.find_element(By.ID, "gdvCourseEQ_cbxHeaderCheckAll").click()
    except NoSuchElementException:
        # Institution has no data
        return {
            "institution": inst_name,
            "city": inst_city,
            "state": inst_state,
            "transfers": [],
        }

    # Open list
    driver.find_element(By.ID, "btnAddToMyEQList").click()
    wait(EC.visibility_of_element_located((By.ID, "gdvMyCourseEQList")))

    # Scrape list
    tds = driver.find_element(By.ID, "gdvMyCourseEQList").find_elements(
        By.TAG_NAME, "td"
    )

    transfer_courses = [
        {
            "transfer": parse_course_td(transfer_course, True),
            "rpi": parse_course_td(rpi_course, False),
            "note": note.text.strip(),
            "begin": begin.text.strip(),
            "end": end.text.strip(),
        }
        for transfer_course, rpi_course, note, begin, end, _ in zip(
            *[iter(x for x in tds)] * 6
        )
    ]

    driver.quit()

    return {
        "institution": inst_name,
        "city": inst_city,
        "state": inst_state,
        "transfers": transfer_courses,
    }


# Scrape course entries. We have a switch to disable including credit counts because the
# RPI-side credit counts are wrong most of the time and this is clarified in notes.
def parse_course_td(td, include_credits):
    # This regex removes spaces next to parentheses. For example,
    #       Calculus II ( 04) -> Calculus II (04)
    td_text = html.unescape(td.get_attribute("innerHTML")).strip().split("<br>")
    courses_info = [
        re.sub(
            "(?<=[\[{(])\s+|\s+(?=[\]})])",
            "",
            x,
        ).split()
        for x in td_text[: len(td_text) - 3]
    ]

    return {
        "catalog": td.find_element(By.TAG_NAME, "span").text.strip(),
        "courses": [parse_one_course(x, include_credits) for x in courses_info],
    }


def parse_one_course(course_info, include_credits):
    # Not all schools use the same course code format, so this figures out how long
    # it is if it exists. It will not exist for Not Transferrable and AP tests.
    try:
        course_id_delim = 1 + list(
            bool(re.search(r"\d", s)) for s in course_info
        ).index(True)
    except ValueError:
        course_id_delim = 1

    # Same deal with credit counts. Fancy logic here to avoid catching course titles
    # with parentheses in them which do not have a credit count, this happened 3 times
    # This also ignores credit counts with "Variable" in them, but ... you try
    try:
        if course_info[-1] == "()":
            cr_delim = -1
        else:
            cr_delim = (
                len(course_info)
                - 1
                - list(
                    bool(re.search(r"^\([.]*[0-9]", s.strip()))
                    for s in course_info[::-1]
                ).index(True)
            )
            assert bool(re.search(r"[0-9]\)", course_info[-1]))
    except (ValueError, AssertionError):
        cr_delim = len(course_info)

    # note serves as a credit count override, since the RPI-side credit counts
    # are inaccurate.
    out = {
        "id": " ".join(course_info[:course_id_delim]),
        "name": normalize_title(" ".join(course_info[course_id_delim:cr_delim])),
    }
    if include_credits:
        out.update({"credits": str(" ".join(course_info[cr_delim:])[1:-1]).strip()}),
    return out


def main():
    global driver
    global options

    if len(sys.argv) != 3:
        print(
            f"USAGE: python {sys.argv[0]} <page number to scrape> <output file>",
            file=sys.stderr,
        )
        return 1

    PAGE_NUM_TO_SCRAPE = int(sys.argv[1])
    OUT_FILENAME = sys.argv[2]

    print(f"Setting up selenium Firefox emulator", file=sys.stderr)
    options = webdriver.FirefoxOptions()
    options.add_argument("--headless")

    user_agent = UserAgent().random
    options.set_preference("general.useragent.override", user_agent)
    print(f"Using randomized user agent {user_agent}", file=sys.stderr)

    with open(OUT_FILENAME, "w") as transferjson:
        json.dump(
            scrape_page(PAGE_NUM_TO_SCRAPE), transferjson, sort_keys=True, indent=2
        )

    driver.quit()


if __name__ == "__main__":
    exit(main())
