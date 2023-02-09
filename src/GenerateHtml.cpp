#include<fstream>
#include<iostream>
#include<filesystem>
#include<json/json.h>
#include"common.h"

struct quatalog_data_t {
        Json::Value terms_offered;
        Json::Value prerequisites;
        Json::Value list_of_terms;
        Json::Value catalog;
};
enum struct TAG { BEGIN, END, INLINE };
bool create_dir_if_not_exist(const fs::path&);
void generate_course_page(const std::string&,const quatalog_data_t&,std::ostream&);
std::string get_course_title(const std::string&,const quatalog_data_t&);
void generate_list(const Json::Value&,const std::string&,const std::string&,const quatalog_data_t&,std::ostream&);
void generate_prereq_display(const Json::Value&,const quatalog_data_t&,std::ostream&);
void generate_course_pill(const std::string&,const quatalog_data_t&,std::ostream&);
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

        for(const auto& course : quatalog_data.catalog.getMemberNames()) {
                std::cerr << course << std::endl;
                generate_course_page(course,quatalog_data,std::cout);
        }
        //generate_course_page("CSCI-4260",quatalog_data,std::cout);
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

void generate_course_page(const std::string& course_id,
                          const quatalog_data_t& quatalog_data,
                          std::ostream& os) {
        std::string course_name, description;
        const auto& catalog_entry = quatalog_data.catalog[course_id];
        const auto& prereqs_entry = quatalog_data.prerequisites[course_id];
        const auto& terms_offered = quatalog_data.terms_offered[course_id];
        const auto& latest_term = terms_offered["latest_term"].asString();
        const auto& credits = terms_offered[latest_term]["credits"];
        const int credMin = credits["min"].asInt(); 
        const int credMax = credits["max"].asInt();
        const auto& credit_string = (credMin == credMax) ? std::to_string(credMin) : (std::to_string(credMin) + "-" + std::to_string(credMax));

        if(catalog_entry) {
                course_name = catalog_entry["name"].asString();
                description = catalog_entry["description"].asString();
        } else {
                course_name = terms_offered[latest_term]["name"].asString();
                description = "This course is not in the most recent catalog."
                                "It may have been discontinued, or had its course "
                                "code changed.";
        }
        const auto& mid_digits = course_id.substr(6,2);
        if(mid_digits == "96" || mid_digits == "97") {
                course_name = "Topics in " + course_id.substr(0,4);
                description = "Course codes between X960 and X979 are for topics courses. "
                                "They are often recycled and used for new/experimental courses.";
        }

        tag(os,TAG::BEGIN,"html");
        tag(os,TAG::BEGIN,"head");
        tag(os,TAG::BEGIN,"title");
        tag(os,TAG::INLINE) << course_id << " - " << course_name << '\n';
        tag(os,TAG::END,"title");
        tag(os,TAG::END,"head");
        tag(os,TAG::BEGIN,"body");
        tag(os,TAG::BEGIN,R"(div id="cd-flex")");
        tag(os,TAG::BEGIN,R"(div id="course-info-container")");
        tag(os,TAG::BEGIN,R"(h1 id="name")");
        tag(os,TAG::INLINE) << course_name << '\n';
        tag(os,TAG::END,"h1");
        tag(os,TAG::INLINE) << R"(<h2 id="code">)" << course_id << "</h2>" << '\n';
        tag(os,TAG::BEGIN,"p");
        tag(os,TAG::INLINE) << description << '\n';
        tag(os,TAG::END,"p");
        tag(os,TAG::BEGIN,R"(div id="cattrs-container")");
        tag(os,TAG::BEGIN,R"(span id="credits-pill" class="attr-pill")");
        tag(os,TAG::INLINE) << credit_string << " " << (credMax == 1 ? "credit" : "credits") << '\n';
        tag(os,TAG::END,"span");
        tag(os,TAG::END,"div");
        generate_list(prereqs_entry["cross_listings"],"Cross-listed with:","crosslist",quatalog_data,os);
        generate_list(prereqs_entry["corequisites"],"Corequisites:","coreq",quatalog_data,os);
        generate_prereq_display(prereqs_entry["prerequisites"],quatalog_data,os);
        tag(os,TAG::END,"div");
        tag(os,TAG::END,"div");
        tag(os,TAG::END,"body");
        tag(os,TAG::END,"html");
}

std::string get_course_title(const std::string& course_id,
                             const quatalog_data_t& quatalog_data) {
        const auto& catalog_entry = quatalog_data.catalog[course_id];
        const auto& terms_offered = quatalog_data.terms_offered[course_id];
        const auto& latest_term = terms_offered["latest_term"].asString();
        if(catalog_entry) {
                return catalog_entry["name"].asString();
        } else {
                return terms_offered[latest_term]["title"].asString();
        }
}

void generate_course_pill(const std::string& course_id,
                          const quatalog_data_t& qlog,
                          std::ostream& os) {
        const auto& title = get_course_title(course_id,qlog);
        tag(os,TAG::INLINE) << R"R(<a class="course-pill" href=")R" << course_id
                           << R"R(.html">)R"
                           << course_id;
        if(!title.empty()) {
                os << " " << title;
        }
        os << "</a>";
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
        if(prereqs.empty()) return;
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
        if(prereq["type"] == "course") {
                std::string course = prereq["course"].asString();
                course[4] = '-';
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
                tag(os,TAG::INLINE) << R"R(<div class="pr-and">and</div>)R" << '\n';
                generate_prereq(prereqs[i],qlog,os);
        }
}

std::ostream& tag(std::ostream& os,enum TAG mode,const std::string& tagname /* = "" */) {
        static int indent_w = 0;
        switch(mode) {
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
        const std::string& INDENT = "        ";
        for(int i = 0;i < indent_w;i++) {
                os << INDENT;
        }
        return os;
}
