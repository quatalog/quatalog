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
bool create_dir_if_not_exist(const fs::path&);
void generate_course_page(const std::string&,const quatalog_data_t&,std::ostream&);
void generate_list(const Json::Value&,const std::string&,const std::string&,const quatalog_data_t&,std::ostream& os);
std::string get_course_title(const std::string&,const quatalog_data_t&);

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

        generate_course_page("LGHT-4830",quatalog_data,std::cout);
        generate_course_page("MATH-4150",quatalog_data,std::cout);
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

        os << R"R(<html>
        <head>
                <title>)R" << course_id << " - " << course_name << R"R(</title>"
        </head>
        <body>
                <div id="cd-flex">
                        <div id="course-info-container">
                                <h1 id="name">)R" << course_name << R"R(</h1>
                                <h2 id="code">)R" << course_id << R"R(</h2>
                                <p>)R" << description << R"R(</p>
                                <div id="cattrs-container">
                                        <span id="credits-pill" class="attr-pill">
                                                <span>)R" << credit_string << " " << (credMax == 1 ? "credit" : "credits") << R"R(</span>
                                        </span>
                                </div>)R" << '\n';
                                generate_list(prereqs_entry["cross_listings"],"Cross-listed with:","crosslist",quatalog_data,os);
                                generate_list(prereqs_entry["corequisites"],"Corequisites:","coreq",quatalog_data,os);
        os << R"R(                        </div>
                </div>
        </body>
</html>)R" << std::endl;
}

std::string get_course_title(const std::string& course_id,const quatalog_data_t& quatalog_data) {
        const auto& catalog_entry = quatalog_data.catalog[course_id];
        const auto& terms_offered = quatalog_data.terms_offered[course_id];
        const auto& latest_term = terms_offered["latest_term"].asString();
        if(catalog_entry) {
                return catalog_entry["name"].asString();
        } else {
                return terms_offered[latest_term]["name"].asString();
        }
}

void generate_list(const Json::Value& list,const std::string& list_name,const std::string& css_prefix,const quatalog_data_t& qlog,std::ostream& os) {
        if(!list.empty()) {
                os << R"R(                                <div id=")R" << css_prefix << R"R(-container" class="hidden">
                                        <div id=")R" << css_prefix << R"R(-title" class="rel-info-title">
                                                )R" << list_name << R"R(
                                        </div>
                                        <div id=")R" << css_prefix << R"R(-classes" class="rel-info-courses">)R";
                for(const auto& cl : list) {
                                const auto& clstr = cl.asString();
                                os << R"R(
                                                <a class="course-pill" href=")R" << clstr
                                   << R"R(.html">)R" 
                                   << clstr << " " << get_course_title(clstr,qlog)
                                   << "</a>\n";
                }
                os << "                                        </div>\n";
                os << "                                </div>\n";
        }
}
