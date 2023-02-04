#include<set>
#include<regex>
#include<fstream>
#include<iostream>
#include<filesystem>
#include<dist/jsoncpp.cpp>

namespace fs = std::filesystem;
void handle_term(const fs::directory_entry&,Json::Value&);
void handle_prefix(const Json::Value&,const std::string&,Json::Value&);
void handle_course(const Json::Value&,const std::string&,Json::Value&);
void handle_attribute(const std::string&,Json::Value&);
void handle_attributes(const Json::Value&,Json::Value&);
void handle_sections(const Json::Value&,Json::Value&);

int main(const int argc,const char** argv) {
        if(argc < 2) {
                std::cerr << "Bad number of arguments " << argc << std::endl;
                return EXIT_FAILURE;
        }

        const auto data_dir_path = fs::path(argv[1]);
        
        if(!fs::is_directory(data_dir_path)) {
                std::cerr << "Data dir argument " << data_dir_path << " is not a directory" << std::endl;
                return EXIT_FAILURE;
        }

        const auto data_dir = fs::directory_iterator(data_dir_path);

        std::set<fs::directory_entry> term_dirs;
        for(const fs::directory_entry term : data_dir) {
                term_dirs.insert(term);
        }

        Json::Value terms_offered;
        terms_offered["all_terms"] = Json::arrayValue;
        for(auto term : term_dirs) {
                if(!fs::is_directory(term)) {
                        continue;
                }

                handle_term(term,terms_offered);
        }

        //std::cout << terms_offered["all_terms"] << std::endl;
        //std::cout << terms_offered["MATH-4020"] << std::endl;
        std::cout << terms_offered << std::endl;

        return EXIT_SUCCESS;
}

void handle_term(const fs::directory_entry& term_entry,
                Json::Value& terms_offered) {
        const fs::path dir = term_entry.path();
        const auto dirname = dir.string();
        const auto term = dir.stem().string();
        const auto courses_filename = dirname + "/courses.json";
        const auto prereqs_filename = dirname + "/prerequisites.json";
        std::fstream courses_file{courses_filename,std::ios::in};
        std::fstream prereqs_file{prereqs_filename,std::ios::in};

#ifdef DEBUG
        std::cerr << '\t' << courses_filename << std::endl;
        std::cerr << '\t' << prereqs_filename << std::endl;
#endif
        std::cerr << "Processing term " << term << "..." << std::endl;
        terms_offered["all_terms"].append(term);

        Json::Value courses;
        courses_file >> courses;
        for(auto prefix : courses) {
                handle_prefix(prefix,term,terms_offered);
        }
}

void handle_prefix(const Json::Value& prefix,const std::string& term,Json::Value& terms_offered) {
#ifdef DEBUG
        std::cerr << "\tProcessing prefix "
                  << prefix["code"]
                  << " - "
                  << prefix["name"]
                  << "..."
                  << std::endl;
#endif
        for(auto course : prefix["courses"]) {
                handle_course(course,term,terms_offered);
        }
}

void handle_course(const Json::Value& course,const std::string& term,Json::Value& terms_offered) {
        const auto course_code = course["id"].asString();
        
        Json::Value sections = course["sections"];

        auto& course_term = terms_offered[course_code][term];
        course_term["title"] = course["title"];

        handle_sections(sections,course_term);
        handle_attributes(course["sections"][0],course_term);
}

void handle_sections(const Json::Value& sections,Json::Value& course_term) {
        int credMin = Json::Value::maxInt, credMax = 0;
        int seatsTaken = 0, capacity = 0, remaining = 0;
        for(auto section : sections) {
                // Get min/max credits *of all sections*
                // (RCOS looking at you)
                credMin = std::min(credMin,section["credMin"].asInt());
                credMax = std::max(credMax,section["credMax"].asInt());
                
                // Add seating data of all sections together.
                // remaining might get clobbered by some sections
                // having negative seats, but this probably won't
                // be too much of an issue
                seatsTaken += section["act"].asInt();
                capacity   += section["cap"].asInt();
                remaining  += section["rem"].asInt();
        }

        course_term["credits"]["min"]     = credMin;
        course_term["credits"]["max"]     = credMax;
        course_term["seats"]["taken"]     = seatsTaken;
        course_term["seats"]["capacity"]  = capacity;
        course_term["seats"]["remaining"] = remaining;
}

void handle_attributes(const Json::Value& section,Json::Value& course_term) {
        // Need to do this because for some reason attributes are by
        // section instead of by course
        
        // This mess is basically C++'s string split but not using
        // as much memory as an actual string split
        const auto delim = std::regex(" and |, ");
        const auto attributes_str = section["attribute"].asString();
        const auto end_itr = std::sregex_token_iterator();
        auto attributes_itr = std::sregex_token_iterator(
                                      attributes_str.begin(),
                                      attributes_str.end(),
                                      delim,
                                      -1
                              );

        // Makes the JSON list of attributes
        while(attributes_itr != end_itr
           && !attributes_itr->str().empty()) {
                handle_attribute(attributes_itr->str(),course_term);
                attributes_itr++;
        }
}

void handle_attribute(const std::string& attribute,Json::Value& course_term) {
        std::string attr;
        if(attribute == "Communication Intensive") {
                attr = "[CI]";
        } else if(attribute == "Writing Intensive") {
                attr = "[WI]";
        } else if(attribute == "HASS Inquiry") {
                attr = "[HInq]";
        } else if(attribute == "Culminating Exp/Capstone") {
                attr = "[CulmExp]";
        } else if(attribute == "PDII Option for Engr Majors") {
                attr = "[PDII]";
        }
        if(!attr.empty()) {
                course_term["attributes"].append(attr);
        }
}
