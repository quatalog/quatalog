import json
import html
import sys
import re
import os.path
import traceback
from time import sleep
import random
from signal import alarm, SIGALRM, signal
from fake_useragent import UserAgent
from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
from selenium.common.exceptions import StaleElementReferenceException
from selenium.common.exceptions import TimeoutException
from selenium.common.exceptions import NoSuchElementException


def raise_(ex):
    raise ex


# Fix course titles accounting for Roman numerals up to X
def normalize_title(input):
    s = " ".join(input.split())
    s = re.sub(r"[A-Za-z]+(['‘’][A-Za-z]+)?", lambda m: m.group(0).capitalize(), s)
    s = re.sub(r"\b(Viii|Vii|Vi|Iv|Ix|Iii|Ii)\b", lambda a: a.group(0).upper(), s)
    return s.strip()


def wait(ec):
    global driver

    WebDriverWait(
        driver, 20, ignored_exceptions=[StaleElementReferenceException]
    ).until(ec)
    sleep(random.uniform(400, 1900) / 1000)


def scrape_course_card(html_id, i, note):
    global driver

    trs = (
        driver.find_element("id", html_id)
        .find_elements(By.CSS_SELECTOR, ".course-detail")[i]
        .find_elements(By.TAG_NAME, "tr")
    )
    course_name_and_id = trs[0].text.split()

    course_desc = ""
    if trs[1].find_element(By.TAG_NAME, "td").get_attribute("colspan") == "2":
        course_desc = trs[1].text

    course_department = normalize_title(
        next((x for x in trs if x.text.strip().startswith("Department:")))
        .find_elements(By.TAG_NAME, "td")[1]
        .text
    )
    course_catalog = normalize_title(
        next((x for x in trs if x.text.strip().startswith("Source catalog:")))
        .find_elements(By.TAG_NAME, "td")[1]
        .text
    )

    try:
        k = 1 + next(
            i for i, v in enumerate(course_name_and_id) if bool(re.search(r"\d", v))
        )
        course_id = " ".join(course_name_and_id[0:k])
        course_name = normalize_title(" ".join(course_name_and_id[k:]))
    except StopIteration:  # Handling for Not Transferrable
        course_id = course_name_and_id[0]
        course_name = normalize_title(" ".join(course_name_and_id[1:]))

    if not note:
        try:
            course_credits = (
                next((x for x in trs if x.text.strip().startswith("Units:")))
                .find_elements(By.TAG_NAME, "td")[1]
                .text.strip()
            )
        except:
            course_credits = ""

        return {
            "id": course_id,
            "name": course_name,
            "credits": course_credits,
            "desc": course_desc,
            "department": course_department,
            "catalog": course_catalog,
        }
    else:
        course_note = driver.find_element("id", "lblCommentsPublic").text.strip()
        return {
            "id": course_id,
            "name": course_name,
            "note": course_note,
            "desc": course_desc,
            "department": course_department,
            "catalog": course_catalog,
        }


def jump_to_page(curr_page, to_page, num_pages, postback_type, pagination_type):
    page = driver.find_element("id", postback_type)
    if num_pages == 1:
        return 1, page
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
        curr_page = int(driver.find_element("id", pagination_type).text.split()[-3])
        if to_page in jumpable_pages:
            jumpable_pages[to_page].click()
            curr_page = to_page
        elif to_page < min(jumpable_pages):
            jumpable_pages[min(jumpable_pages)].click()
            curr_page = min(jumpable_pages)
        else:
            jumpable_pages[max(jumpable_pages)].click()
            curr_page = max(jumpable_pages)
        print(f"Jumping to {postback_type} page {curr_page}", file=sys.stderr)

        wait(EC.staleness_of(page))
        sleep(random.uniform(3, 6))
        page = driver.find_element("id", postback_type)
    return curr_page, page


def main():
    global driver

    if len(sys.argv) != 3 and len(sys.argv) != 4:
        print(
            f"USAGE: python {sys.argv[0]} <transfer file> <state file> [timeout minutes]"
        )
        exit(1)

    transfer_json_path = sys.argv[1]
    state_json_path = sys.argv[2]
    timeout_seconds = int(sys.argv[3] if len(sys.argv) == 4 else 120) * 60

    # Set up timeout so that the GH action does not run forever, pretend it's ^C
    print(f"Setting timeout to {timeout_seconds} seconds", file=sys.stderr)
    signal(SIGALRM, lambda a, b: raise_(KeyboardInterrupt))
    alarm(timeout_seconds)

    options = webdriver.FirefoxOptions()
    options.add_argument("--headless")

    user_agent = UserAgent().random
    options.set_preference("general.useragent.override", user_agent)
    # options.set_preference("network.proxy.socks", "")
    # options.set_preference("network.proxy.socks_port", )
    # options.set_preference("network.proxy.socks_remote_dns", True)
    # options.set_preference("network.proxy.type", 1)
    print(f"Using randomized user agent {user_agent}", file=sys.stderr)

    driver = webdriver.Firefox(options=options)
    driver.get(
        "https://tes.collegesource.com/publicview/TES_publicview01.aspx?rid=f080a477-bff8-46df-a5b2-25e9affdd4ed&aid=27b576bb-cd07-4e57-84d0-37475fde70ce"
    )

    num_pages = int(
        driver.find_element("id", "lblInstWithEQPaginationInfo").text.split()[-1]
    )
    print(f"{num_pages} pages detected", file=sys.stderr)

    state = {"inst_pg": 1, "inst_idx": 0, "course_pg": 1, "course_idx": 0}
    institutions = {}
    if os.path.isfile(state_json_path):
        with open(state_json_path, "r") as statejson:
            state = json.load(statejson)
    if os.path.isfile(transfer_json_path):
        with open(transfer_json_path, "r") as transferjson:
            institutions = json.load(transferjson)

    print("Loaded state: ", end="", file=sys.stderr)
    json.dump(state, sys.stderr, indent=4)
    print("", file=sys.stderr)

    try:
        curr_inst_page = 1
        while state["inst_pg"] <= num_pages:
            curr_inst_page, page = jump_to_page(
                curr_inst_page,
                state["inst_pg"],
                num_pages,
                "gdvInstWithEQ",
                "lblInstWithEQPaginationInfo",
            )

            inst_list_len = len(
                page.find_elements(
                    By.CSS_SELECTOR, "a[id^=gdvInstWithEQ_btnCreditFromInstName_]"
                )
            )

            while state["inst_idx"] < inst_list_len:
                institution_link = driver.find_element(
                    "id", "gdvInstWithEQ"
                ).find_elements(
                    By.CSS_SELECTOR, "a[id^=gdvInstWithEQ_btnCreditFromInstName_]"
                )[
                    state["inst_idx"]
                ]
                fields = institution_link.find_element(By.XPATH, "../..").find_elements(
                    By.CSS_SELECTOR, ".gdv_boundfield_uppercase"
                )
                inst_name = normalize_title(institution_link.text)
                city = normalize_title(fields[0].text)
                us_state = fields[1].text.strip()

                institution_link.click()
                wait(EC.staleness_of(institution_link))

                try:
                    course_pages_len = int(
                        driver.find_element(
                            "id", "lblCourseEQPaginationInfo"
                        ).text.split()[-1]
                    )
                except NoSuchElementException:
                    course_pages_len = 1

                try:
                    courses = institutions[inst_name]["courses"]
                except Exception:
                    courses = []

                curr_course_page = 1
                while state["course_pg"] <= course_pages_len:
                    curr_course_page, page = jump_to_page(
                        curr_course_page,
                        state["course_pg"],
                        course_pages_len,
                        "gdvCourseEQ",
                        "lblCourseEQPaginationInfo",
                    )

                    course_links_len = len(
                        page.find_elements(
                            By.CSS_SELECTOR, "a[id^=gdvCourseEQ_btnViewCourseEQDetail_]"
                        )
                    )

                    while state["course_idx"] < course_links_len:
                        course_link = driver.find_element(
                            "id", "gdvCourseEQ"
                        ).find_elements(
                            By.CSS_SELECTOR, "a[id^=gdvCourseEQ_btnViewCourseEQDetail_]"
                        )[
                            state["course_idx"]
                        ]
                        course_link.click()

                        try:
                            wait(
                                EC.element_to_be_clickable(
                                    (By.CSS_SELECTOR, ".modal-header button")
                                ),
                            )

                            transfer = [
                                scrape_course_card("lblSendCourseEQDetail", i, False)
                                for i in range(
                                    0,
                                    len(
                                        driver.find_element(
                                            "id", "lblSendCourseEQDetail"
                                        ).find_elements(
                                            By.CSS_SELECTOR, ".course-detail"
                                        )
                                    ),
                                )
                            ]

                            rpi = [
                                scrape_course_card("lblReceiveCourseEQDetail", i, True)
                                for i in range(
                                    0,
                                    len(
                                        driver.find_element(
                                            "id", "lblReceiveCourseEQDetail"
                                        ).find_elements(
                                            By.CSS_SELECTOR, ".course-detail"
                                        )
                                    ),
                                )
                            ]

                            print(
                                f"{inst_name} ({state['inst_idx']}:{state['inst_pg']}/{num_pages}): {transfer[0]['id']} {transfer[0]['name']} -> {rpi[0]['id']} {rpi[0]['name']} ({state['course_idx']}:{state['course_pg']}/{course_pages_len})",
                                file=sys.stderr,
                            )

                            begin_date = driver.find_element(
                                "id", "lblBeginEffectiveDate"
                            ).text
                            end_date = driver.find_element(
                                "id", "lblEndEffectiveDate"
                            ).text

                            driver.find_element(
                                By.CSS_SELECTOR, ".modal-header button"
                            ).click()

                            courses += [
                                {
                                    "transfer": transfer,
                                    "rpi": rpi,
                                    "begin": begin_date,
                                    "end": end_date,
                                }
                            ]
                            state["course_idx"] += 1
                        except (Exception, KeyboardInterrupt) as e:
                            institutions.update(
                                {
                                    inst_name: {
                                        "city": city,
                                        "state": us_state,
                                        "courses": courses,
                                    }
                                }
                            )
                            raise e
                    state["course_idx"] = 0
                    state["course_pg"] += 1

                institutions.update(
                    {inst_name: {"city": city, "state": us_state, "courses": courses}}
                )
                state["course_pg"] = 1
                state["inst_idx"] += 1

                driver.find_element("id", "btnSwitchView").click()
                wait(
                    EC.text_to_be_present_in_element(
                        ("id", "lblInstWithEQPaginationInfo"), str(state["inst_pg"])
                    ),
                )
            state["inst_idx"] = 0
            state["inst_pg"] = (state["inst_pg"] % num_pages) + 1

    except (Exception, KeyboardInterrupt) as e:
        print("Program hits exception and will save and terminate", file=sys.stderr)
        print(traceback.format_exc(), file=sys.stderr)

    print("Program will terminate with state: ", end="", file=sys.stderr)
    json.dump(state, sys.stderr, indent=4)
    print("", file=sys.stderr)
    with open(transfer_json_path, "w") as transferjson:
        json.dump(institutions, transferjson, indent=4)
    with open(state_json_path, "w") as statejson:
        json.dump(state, statejson, indent=4)
    driver.quit()


if __name__ == "__main__":
    main()
