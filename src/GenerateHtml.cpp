#include<regex>
#include<fstream>
#include<iostream>
#include<filesystem>
#include<unordered_set>
#include<unordered_map>
#include<json/json.h>
namespace fs = std::filesystem;

struct quatalog_data_t {
        Json::Value terms_offered;
        Json::Value prerequisites;
        Json::Value list_of_terms;
        Json::Value catalog;
};
enum struct TAG { BEGIN, END, INLINE, COMPLEX_BEGIN };
enum struct TERM { SPRING, SUMMER, SUMMER2, SUMMER3, FALL, WINTER };
enum struct OFFERED { YES, NO, DIFF_CODE, UNSCHEDULED };
const std::unordered_map<enum OFFERED,std::string> offered_to_string {
        { OFFERED::YES,"offered" },
        { OFFERED::NO,"not-offered" },
        { OFFERED::DIFF_CODE,"offered-diff-code" },
        { OFFERED::UNSCHEDULED,"unscheduled" }
};
const std::unordered_map<enum TERM,std::string> term_to_string {
        { TERM::SPRING,"spring" },
        { TERM::SUMMER,"summer" },
        { TERM::SUMMER2,"summer2" },
        { TERM::SUMMER3,"summer3" },
        { TERM::FALL,"fall" },
        { TERM::WINTER,"winter" }
};
const std::unordered_map<enum TERM,std::string> term_to_number {
        { TERM::SPRING,"01" },
        { TERM::SUMMER,"05" },
        { TERM::SUMMER2,"0502" },
        { TERM::SUMMER3,"0503" },
        { TERM::FALL,"09" },
        { TERM::WINTER,"12" }
};
std::unordered_set<std::string> get_all_courses(const quatalog_data_t&);
std::string fix_course_ids(std::string);
bool create_dir_if_not_exist(const fs::path&);
const Json::Value& get_data(const Json::Value&,std::string);
void generate_course_page(const std::string&,const quatalog_data_t&,std::ostream&);
void get_prerequisites(const quatalog_data_t&,std::string);
void generate_opt_container(std::ostream&);
std::string generate_credit_string(const Json::Value& credits);
std::string get_course_title(const std::string&,const quatalog_data_t&);
void generate_years_table(const Json::Value&,const Json::Value&,const quatalog_data_t&,std::ostream&);
void generate_year_row(const int,const Json::Value&,const Json::Value&,const quatalog_data_t&,std::ostream&);
bool is_term_scheduled(const std::string&,const quatalog_data_t&);
enum OFFERED is_course_offered(const int,const enum TERM,const Json::Value&,const Json::Value&,const quatalog_data_t&);
void generate_table_cell(const int,const enum TERM,const Json::Value&,const enum OFFERED,std::ostream&);
void generate_attributes(const Json::Value&,std::ostream&);
void generate_list(const Json::Value&,const std::string&,const std::string&,const quatalog_data_t&,std::ostream&);
void generate_prereq_display(const Json::Value&,const quatalog_data_t&,std::ostream&);
void generate_course_pill(std::string,const quatalog_data_t&,std::ostream&);
void generate_prereq(const Json::Value&,const quatalog_data_t&,std::ostream&);
void generate_or_prereq(const Json::Value&,const quatalog_data_t&,std::ostream&);
void generate_and_prereq(const Json::Value&,const quatalog_data_t&,std::ostream&);
std::ostream& indent(std::ostream&,const int);
std::ostream& tag(std::ostream&,enum TAG,const std::string& = "");

int main(const int argc,
         const char** argv) {
        if(argc != 6) {
                std::cerr << "Bad number of arguments (" << argc << ")" << std::endl;
                std::cerr << "Usage: " << argv[0]
                        << " <terms_offered_file>"
                        << " <prerequisites_file>"
                        << " <list_of_terms_file>"
                        << " <catalog_file>"
                        << " <out_directory>"
                        << std::endl;
                return EXIT_FAILURE;
        }
        const auto& terms_offered_filename = std::string(argv[1]);
        const auto& prerequisites_filename = std::string(argv[2]);
        const auto& list_of_terms_filename = std::string(argv[3]);
        const auto& catalog_filename = std::string(argv[4]);
        const auto& out_dir_path = fs::path(argv[5]);

        if(!create_dir_if_not_exist(out_dir_path)) {
                std::cerr << "What" << std::endl;
                return EXIT_FAILURE;
        }

        std::fstream terms_offered_file{terms_offered_filename,std::ios::in};
        std::fstream prerequisites_file{prerequisites_filename,std::ios::in};
        std::fstream list_of_terms_file{list_of_terms_filename,std::ios::in};
        std::fstream catalog_file{catalog_filename,std::ios::in};

        quatalog_data_t quatalog_data;
        terms_offered_file >> quatalog_data.terms_offered;
        prerequisites_file >> quatalog_data.prerequisites;
        list_of_terms_file >> quatalog_data.list_of_terms;
        catalog_file >> quatalog_data.catalog;

        auto courses = get_all_courses(quatalog_data);
        for(const auto& course : courses) {
                const auto& html_path = out_dir_path / (course + ".html");
                auto file = std::ofstream(html_path);
                generate_course_page(course,quatalog_data,file);
        }
}

std::unordered_set<std::string> get_all_courses(const quatalog_data_t& qlog) {
        std::unordered_set<std::string> output;
        for(const std::string& course : qlog.catalog.getMemberNames()) {
                if(course.length() != 9) continue; 
                output.insert(fix_course_ids(course));
        }
        for(const std::string& course : qlog.terms_offered.getMemberNames()) {
                output.insert(fix_course_ids(course));
        }
        return output;
}

std::string fix_course_ids(std::string course) {
        course[4] = '-';
        if(course.substr(0,3) == "STS") {
                course[3] = 'O';
        }
        return course;
}

bool create_dir_if_not_exist(const fs::path& path) {
        if(fs::exists(path)) {
                if(!fs::is_directory(path)) {
                        std::cerr << "out_directory argument "
                                << path
                                << "is not a directory"
                                << std::endl;
                        return false;
                }
                return true;
        }

        return fs::create_directory(path);
}

const Json::Value& get_data(const Json::Value& data,
                            std::string course_id) {
        course_id[4] = '-';
        if(course_id.substr(0,3) != "STS") {
                return data[course_id];
        }
        const auto& stso = data[course_id];
        if(stso) return stso;
        course_id[3] = 'S';
        const auto& stss = data[course_id];
        if(stss) return stss;
        course_id[3] = 'H';
        const auto& stsh = data[course_id];
        return stsh;
}

void generate_course_page(const std::string& course_id,
                          const quatalog_data_t& quatalog_data,
                          std::ostream& os) {
        std::cerr << "Generating course page for " << course_id << "..." << std::endl;
        std::string course_name, description;
        const auto& catalog_entry = quatalog_data.catalog[course_id];
        const auto& prereqs_entry = get_data(quatalog_data.prerequisites,course_id);
        const auto& terms_offered = get_data(quatalog_data.terms_offered,course_id);
        const auto& latest_term = terms_offered["latest_term"].asString();
        const auto& credits = terms_offered[latest_term]["credits"];
        const auto& credit_string = generate_credit_string(credits);

        if(catalog_entry) {
                course_name = catalog_entry["name"].asString();
                description = catalog_entry["description"].asString();
        } else {
                course_name = terms_offered[latest_term]["name"].asString();
                description = "This course is not in the most recent catalog. "
                                "It may have been discontinued, had its course "
                                "code changed, or just not be in the catalog for "
                                "some other reason.";
        }
        const auto& mid_digits = course_id.substr(6,2);
        if(mid_digits == "96" || mid_digits == "97") {
                course_name = "Topics in " + course_id.substr(0,4);
                description = "Course codes between X960 and X979 are for topics courses. "
                                "They are often recycled and used for new/experimental courses.";
        }

        const std::regex escape_string(R"(")");
        const std::string& description_meta = std::regex_replace(description,escape_string,"&quot;");

        tag(os,TAG::BEGIN,"html");
        tag(os,TAG::BEGIN,"head");
        tag(os,TAG::BEGIN,"title");
        tag(os,TAG::INLINE) << course_id << " - " << course_name << '\n';
        tag(os,TAG::END,"title");
        tag(os,TAG::INLINE) << R"(<meta property="og:title" content=")" << course_id << " - " << course_name << R"(">)" << '\n';
        tag(os,TAG::INLINE) << R"(<meta property="og:description" content=")" << description_meta << R"(">)" << '\n';
        tag(os,TAG::INLINE) << R"(<link rel="stylesheet" href="../css/common.css">)" << '\n';
        tag(os,TAG::INLINE) << R"(<link rel="stylesheet" href="../css/coursedisplay.css">)" << '\n';
        tag(os,TAG::END,"head");
        tag(os,TAG::BEGIN,R"(body class="search_plugin_added")");
        tag(os,TAG::BEGIN,R"(div id="qlog-header")");
        tag(os,TAG::INLINE) << R"(<a id="qlog-wordmark" href="./"><svg><use href="./images/quatalogHWordmark.svg#QuatalogHWordmark"></use></svg></a>)" << '\n';
        tag(os,TAG::INLINE) << R"R(<input type="text" id="header-search" placeholder="Search..." onkeydown="prepSearch(this, event)">)R" << '\n';
        tag(os,TAG::END,"div");
        tag(os,TAG::BEGIN,R"(div id="cd-flex")");
        tag(os,TAG::BEGIN,R"(div id="course-info-container")");
        tag(os,TAG::BEGIN,R"(h1 id="name")");
        tag(os,TAG::INLINE) << course_name << '\n';
        tag(os,TAG::END,"h1");
        tag(os,TAG::BEGIN,R"(h2 id="code")");
        tag(os,TAG::INLINE) << course_id << '\n';
        tag(os,TAG::END,"h2");
        tag(os,TAG::BEGIN,"p");
        tag(os,TAG::INLINE) << description << '\n';
        tag(os,TAG::END,"p");
        tag(os,TAG::BEGIN,R"(div id="cattrs-container")");
        tag(os,TAG::BEGIN,R"(span id="credits-pill" class="attr-pill")");
        tag(os,TAG::INLINE) << credit_string << " " << (credits["credMax"].asInt() == 1 ? "credit" : "credits") << '\n';
        tag(os,TAG::END,"span");
        generate_attributes(prereqs_entry["attributes"],os);
        tag(os,TAG::END,"div");
        generate_list(prereqs_entry["cross_listings"],"Cross-listed with:","crosslist",quatalog_data,os);
        generate_list(prereqs_entry["corequisites"],"Corequisites:","coreq",quatalog_data,os);
        generate_prereq_display(prereqs_entry["prerequisites"],quatalog_data,os);
        tag(os,TAG::END,"div");
        tag(os,TAG::BEGIN,R"(div id="past-container")");
        tag(os,TAG::BEGIN,R"(h2 id="past-title")");
        tag(os,TAG::INLINE) << "Past Term Data" << '\n';
        tag(os,TAG::END,"h2");
        tag(os,TAG::INLINE) << R"(<input type="radio" id="simple-view-input" name="view-select" value="simple" checked="checked">)" << '\n';
        tag(os,TAG::INLINE) << R"(<input type="radio" id="detail-view-input" name="view-select" value="detailed">)" << '\n';
        generate_opt_container(os);
        generate_years_table(terms_offered,prereqs_entry["cross_listings"],quatalog_data,os);
        tag(os,TAG::END,"div");
        tag(os,TAG::END,"div");
        tag(os,TAG::END,"body");
        tag(os,TAG::END,"html");
}

std::string generate_credit_string(const Json::Value& credits) {
        const int credMin = credits["min"].asInt();
        const int credMax = credits["max"].asInt();
        return (credMin == credMax) ? std::to_string(credMin) : (std::to_string(credMin) + "-" + std::to_string(credMax));
}

void generate_years_table(const Json::Value& terms_offered,
                          const Json::Value& cross_listings,
                          const quatalog_data_t& qlog,
                          std::ostream& os) {
        tag(os,TAG::BEGIN,"table");
        tag(os,TAG::BEGIN,"thead");

        tag(os,TAG::BEGIN,"tr");
        tag(os,TAG::INLINE) << "<th></th>" << '\n';
        tag(os,TAG::INLINE) << R"(<th class="spring season-label">Spring</th>)" << '\n';
        tag(os,TAG::INLINE) << R"(<th class="summer season-label" colspan="2">Summer</th>)" << '\n';
        tag(os,TAG::INLINE) << R"(<th class="fall season-label">Fall</th>)" << '\n';
        //tag(os,TAG::INLINE) << R"(<th class="winter season-label">Enrichment</th>)" << '\n';
        tag(os,TAG::END,"tr");
        tag(os,TAG::BEGIN,"tr");
        tag(os,TAG::INLINE) << R"(<th colspan="2"></th>)" << '\n';
        tag(os,TAG::INLINE) << R"(<th class="summer2 midsum-label">(Session 1)</th>)" << '\n';
        tag(os,TAG::INLINE) << R"(<th class="summer3 midsum-label">(Session 2)</th>)" << '\n';
        //tag(os,TAG::INLINE) << R"(<th colspan="2"></th>)" << '\n';
        tag(os,TAG::INLINE) << R"(<th></th>)" << '\n';
        tag(os,TAG::END,"tr");

        tag(os,TAG::END,"thead");
        tag(os,TAG::BEGIN,"tbody");

        const int current_year = std::stoi(qlog.list_of_terms["current_term"].asString().substr(0,4));
        const int oldest_year = std::stoi(qlog.list_of_terms["oldest_term"].asString().substr(0,4));
        for(int year = current_year;year >= oldest_year;year--) {
                generate_year_row(year,terms_offered,cross_listings,qlog,os);
        }

        tag(os,TAG::END,"tbody");
        tag(os,TAG::END,"table");
}

void generate_year_row(const int year,
                       const Json::Value& terms_offered,
                       const Json::Value& cross_listings,
                       const quatalog_data_t& qlog,
                       std::ostream& os) {
        tag(os,TAG::BEGIN,"tr");
        tag(os,TAG::INLINE) << R"(<th class="year">)" << year << "</th>" << '\n';

        generate_table_cell(year,TERM::SPRING,terms_offered,is_course_offered(year,TERM::SPRING,terms_offered,cross_listings,qlog),os);

        const enum OFFERED summer1 = is_course_offered(year,TERM::SUMMER,terms_offered,cross_listings,qlog);
        if(summer1 != OFFERED::NO) {
                generate_table_cell(year,TERM::SUMMER,terms_offered,summer1,os);
        } else {
                const enum OFFERED summer2 = is_course_offered(year,TERM::SUMMER2,terms_offered,cross_listings,qlog);
                const enum OFFERED summer3 = is_course_offered(year,TERM::SUMMER3,terms_offered,cross_listings,qlog);
                if((summer2 == OFFERED::NO || summer2 == OFFERED::UNSCHEDULED)
                && (summer3 == OFFERED::NO || summer3 == OFFERED::UNSCHEDULED)) {
                        generate_table_cell(year,TERM::SUMMER,terms_offered,summer1,os);
                } else {
                        generate_table_cell(year,TERM::SUMMER2,terms_offered,summer2,os);
                        generate_table_cell(year,TERM::SUMMER3,terms_offered,summer3,os);
                }
        }

        generate_table_cell(year,TERM::FALL,terms_offered,is_course_offered(year,TERM::FALL,terms_offered,cross_listings,qlog),os);
        //generate_table_cell(year,TERM::WINTER,terms_offered,is_course_offered(year,TERM::WINTER,terms_offered,cross_listings,qlog),os);

        tag(os,TAG::END,"tr");
}

void generate_table_cell(const int year,
                         const enum TERM term,
                         const Json::Value& terms_offered,
                         const enum OFFERED is_offered,
                         std::ostream& os) {
        std::string year_term = std::to_string(year) + term_to_number.at(term);
        const auto& term_offered = terms_offered[year_term];
        const auto& course_title = term_offered["title"].asString();
        const auto& credit_string = generate_credit_string(term_offered["credits"]);

        tag(os,TAG::COMPLEX_BEGIN) << R"(<td )";
        if(term == TERM::SUMMER) {
                os << R"(colspan="2" )";
        }
        os << R"(class="term )"
                << term_to_string.at(term) << ' '
                << offered_to_string.at(is_offered)
                << R"(">)" << '\n';
        if(is_offered == OFFERED::YES) {
                tag(os,TAG::BEGIN,R"(div class="view-container detail-view-container")");
                tag(os,TAG::BEGIN,R"(span class="term-course-info")");

                tag(os,TAG::INLINE) << course_title << " (" << credit_string << "c)";
                for(const auto& attr : term_offered["attributes"]) {
                        os << ' ' << attr.asString();
                }
                os << '\n';

                tag(os,TAG::END,"span");
                tag(os,TAG::BEGIN,R"(ul class="prof-list")");
                for(const auto& instructor : term_offered["instructors"]) {
                        tag(os,TAG::INLINE) << "<li>" << instructor.asString() << "</li>" << '\n';
                }
                tag(os,TAG::END,"ul");
                tag(os,TAG::BEGIN,R"(span class="course-capacity")");
                const auto& seats = term_offered["seats"];
                tag(os,TAG::INLINE) << "Seats Taken: " << seats["taken"] << '/' << seats["capacity"] << '\n';
                tag(os,TAG::END,"span");
                tag(os,TAG::END,"div");
        }
        tag(os,TAG::END,"td");
}

bool is_term_scheduled(const std::string& term_str,
                       const quatalog_data_t& qlog) {
        static std::unordered_set<std::string> terms;
        if(terms.empty()) {
                for(const auto& term : qlog.list_of_terms["all_terms"]) {
                        terms.insert(term.asString());
                }
        }
        return terms.count(term_str);
}

enum OFFERED is_course_offered(const int year,
                               const enum TERM term,
                               const Json::Value& terms_offered,
                               const Json::Value& cross_listings,
                               const quatalog_data_t& qlog) {
        const std::string& term_str = std::to_string(year) + term_to_number.at(term);
        if(!is_term_scheduled(term_str,qlog)) {
                return OFFERED::UNSCHEDULED;
        } else if(terms_offered.isMember(term_str)) {
                return OFFERED::YES;
        } else {
                for(const auto& cl : cross_listings) {
                        if(get_data(terms_offered,cl.asString())) {
                                return OFFERED::DIFF_CODE;
                        }
                }
                return OFFERED::NO;
        }
}

void generate_opt_container(std::ostream& os) {
        tag(os,TAG::BEGIN,R"(div id="opt-container")");
        tag(os,TAG::BEGIN,R"(div id="key-panel")");
        tag(os,TAG::BEGIN,R"(div id="yes-code" class="key-code")");
        tag(os,TAG::BEGIN,R"(span class="code-icon" id="yes-code-icon")");
        tag(os,TAG::INLINE) << R"(<svg><use href="./icons.svg#circle-check"></use></svg>)" << '\n';
        tag(os,TAG::END,"span");
        tag(os,TAG::INLINE) << "Offered" << '\n';
        tag(os,TAG::END,"div");
        tag(os,TAG::BEGIN,R"(div id="no-code" class="key-code")");
        tag(os,TAG::BEGIN,R"(span class="code-icon" id="no-code-icon")");
        tag(os,TAG::INLINE) << R"(<svg><use href="./icons.svg#circle-no"></use></svg>)" << '\n';
        tag(os,TAG::END,"span");
        tag(os,TAG::INLINE) << "Not Offered" << '\n';
        tag(os,TAG::END,"div");
        tag(os,TAG::BEGIN,R"(div id="diff-code" class="key-code")");
        tag(os,TAG::BEGIN,R"(span class="code-icon" id="diff-code-icon")");
        tag(os,TAG::INLINE) << R"(<svg><use href="./icons.svg#circle-question"></use></svg>)" << '\n';
        tag(os,TAG::END,"span");
        tag(os,TAG::INLINE) << "Offered as Cross-Listing Only" << '\n';
        tag(os,TAG::END,"div");
        tag(os,TAG::BEGIN,R"(div id="nil-code" class="key-code")");
        tag(os,TAG::BEGIN,R"(span class="code-icon" id="nil-code-icon")");
        tag(os,TAG::INLINE) << R"(<svg><use href="./icons.svg#circle-empty"></use></svg>)" << '\n';
        tag(os,TAG::END,"span");
        tag(os,TAG::INLINE) << "No Term Data" << '\n';
        tag(os,TAG::END,"div");
        tag(os,TAG::END,"div");
        tag(os,TAG::BEGIN,R"(div id="control-panel")");
        tag(os,TAG::BEGIN,R"(label for="simple-view-input" id="simple-view-label" class="view-option-label")");
        tag(os,TAG::BEGIN,R"(span class="view-icon" id="simple-view-icon")");
        tag(os,TAG::INLINE) << R"(<span class="view-icon-selected"><svg><use href="./icons.svg#circle-dot"></use></svg></span>)" << '\n';
        tag(os,TAG::INLINE) << R"(<span class="view-icon-unselected"><svg><use href="./icons.svg#circle-empty"></use></svg></span>)" << '\n';
        tag(os,TAG::END,"span");
        tag(os,TAG::INLINE) << "Simple View" << '\n';
        tag(os,TAG::END,"label");
        tag(os,TAG::BEGIN,R"(label for="detail-view-input" id="detail-view-label" class="view-option-label")");
        tag(os,TAG::BEGIN,R"(span class="view-icon" id="detail-view-icon")");
        tag(os,TAG::INLINE) << R"(<span class="view-icon-selected"><svg><use href="./icons.svg#circle-dot"></use></svg></span>)" << '\n';
        tag(os,TAG::INLINE) << R"(<span class="view-icon-unselected"><svg><use href="./icons.svg#circle-empty"></use></svg></span>)" << '\n';
        tag(os,TAG::END,"span");
        tag(os,TAG::INLINE) << "Detailed View" << '\n';
        tag(os,TAG::END,"label");
        tag(os,TAG::END,"div");
        tag(os,TAG::END,"div");
}

std::string get_course_title(const std::string& course_id,
                             const quatalog_data_t& quatalog_data) {
        const auto& catalog_entry = get_data(quatalog_data.catalog,course_id);
        const auto& terms_offered = get_data(quatalog_data.terms_offered,course_id);
        const auto& latest_term = terms_offered["latest_term"].asString();
        if(catalog_entry) {
                return catalog_entry["name"].asString();
        } else {
                return terms_offered[latest_term]["title"].asString();
        }
}

void generate_course_pill(std::string course_id,
                          const quatalog_data_t& qlog,
                          std::ostream& os) {
        if(course_id.substr(0,3) == "STS") {
                course_id[3] = 'O';
        }
        course_id[4] = '-';
        const auto& title = get_course_title(course_id,qlog);
        tag(os,TAG::INLINE) << R"(<a class="course-pill" href=")" << course_id
                           << R"(.html">)"
                           << course_id;
        if(!title.empty()) {
                os << " " << title;
        }
        os << "</a>";
}

void generate_attributes(const Json::Value& attributes,
                         std::ostream& os) {
        for(const auto& attribute : attributes) {
                tag(os,TAG::BEGIN,R"(span class="attr-pill")");
                tag(os,TAG::INLINE) << attribute.asString() << '\n';
                tag(os,TAG::END,"span");
        }
}

void generate_list(const Json::Value& list,
                   const std::string& list_name,
                   const std::string& css_prefix,
                   const quatalog_data_t& qlog,
                   std::ostream& os) {
        if(list.empty()) return;
        tag(os,TAG::BEGIN,R"(div id=")" + css_prefix + R"(-container")");
        tag(os,TAG::BEGIN,R"(div id=")" + css_prefix + R"(-title" class="rel-info-title")");
        tag(os,TAG::INLINE) << list_name << '\n';
        tag(os,TAG::END,"div");
        tag(os,TAG::BEGIN,"div id=" + css_prefix + R"(-classes" class="rel-info-courses")");
        for(const auto& cl : list) {
                generate_course_pill(cl.asString(),qlog,os);
                os << '\n';
        }
        tag(os,TAG::END,"div");
        tag(os,TAG::END,"div");
}

void generate_prereq_display(const Json::Value& prereqs,const quatalog_data_t& qlog,std::ostream& os) {
        tag(os,TAG::BEGIN,R"(div id="prereq-container" class="rel-info-container")");
        tag(os,TAG::BEGIN,R"(div id="prereq-title" class="rel-info-title")");
        tag(os,TAG::INLINE) << "Prereqs:" << '\n';
        tag(os,TAG::END,"div");
        tag(os,TAG::BEGIN,R"(div id="prereq-classes" class="rel-info-courses")");
        generate_prereq(prereqs,qlog,os);
        tag(os,TAG::END,"div");
        tag(os,TAG::END,"div");
}

void generate_prereq(const Json::Value& prereq,
                     const quatalog_data_t& qlog,
                     std::ostream& os) {
        if(prereq.empty()) {
                tag(os,TAG::BEGIN,R"(span class="none-rect")");
                tag(os,TAG::INLINE) << "none" << '\n';
                tag(os,TAG::END,"span");
        } else if(prereq["type"] == "course") {
                std::string course = prereq["course"].asString();
                generate_course_pill(course,qlog,os);
                os << '\n';
        } else if(prereq["type"] == "or") {
                generate_or_prereq(prereq["nested"],qlog,os);
        } else if(prereq["type"] == "and") {
                generate_and_prereq(prereq["nested"],qlog,os);
        }
}

void generate_or_prereq(const Json::Value& prereqs,
                        const quatalog_data_t& qlog,
                        std::ostream& os) {
        tag(os,TAG::BEGIN,R"(div class="pr-or-con")");
        tag(os,TAG::BEGIN,R"(div class="pr-or-title")");
        tag(os,TAG::INLINE) << "one of:" << '\n';
        tag(os,TAG::END,"div");
        tag(os,TAG::BEGIN,R"(div class="pr-or")");
        for(const auto& pr : prereqs) {
                generate_prereq(pr,qlog,os);
        }
        tag(os,TAG::END,"div");
        tag(os,TAG::END,"div");
}

void generate_and_prereq(const Json::Value& prereqs,
                         const quatalog_data_t& qlog,
                         std::ostream& os) {
        generate_prereq(prereqs[0],qlog,os);
        for(Json::Value::ArrayIndex i = 1; i != prereqs.size();i++) {
                tag(os,TAG::INLINE) << R"(<div class="pr-and">and</div>)" << '\n';
                generate_prereq(prereqs[i],qlog,os);
        }
}

std::ostream& tag(std::ostream& os,enum TAG mode,const std::string& tagname /* = "" */) {
        static int indent_w = 0;
        switch(mode) {
        case TAG::COMPLEX_BEGIN:
                return indent(os,indent_w++);
        case TAG::INLINE:
                return indent(os,indent_w);
        case TAG::BEGIN:
                return indent(os,indent_w++) << '<' << tagname << '>' << '\n';
        case TAG::END:
                return indent(os,--indent_w) << "</" << tagname << '>' << '\n';
        }
}

std::ostream& indent(std::ostream& os,
                     const int indent_w) {
        const std::string& INDENT = "  ";
        for(int i = 0;i < indent_w;i++) {
                os << INDENT;
        }
        return os;
}
