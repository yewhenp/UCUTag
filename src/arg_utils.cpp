#include "arg_utils.h"


std::map<std::string, std::string> parse_args(int argc, char **argv) {
    std::string usage = "USAGE:\n    ucutag  [ -n--name fs_name=main ] [--help ] [-m|--mount] mountpoint";
    std::map<std::string, std::string> result{};
    bool debug;
    // parse arguments
    try {
        po::options_description generic("Generic options");
        generic.add_options()
                ("help,h", "Show this message and exit")
                ("version,v", "program version")
                ("name,n", po::value<std::string>(), "Name of file system")
                ("debug,d", po::bool_switch(&debug), "Debug. Compile with Debug to see debug messages");

        po::options_description hidden("Hidden options");
        hidden.add_options()
                ("mount,m", po::value<std::string>(), "input script");

        po::options_description cmdline_options;
        cmdline_options.add(generic).add(hidden);

        po::options_description visible;
        visible.add(generic);

        po::positional_options_description p;
        p.add("mount", 1);

        po::variables_map vm;
        store(po::command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), vm);
        notify(vm);

        if (vm.count("help")) {
            std::cout << usage << std::endl;
            exit(0);
        }

        if (vm.count("version")) {
            std::cout << "UCUTag: tag oriented file system, version 1.0-1" << std::endl;
            exit(0);
        }

        if (!vm.count("mount")) {
            std::cerr << "Error: Mount point is not specified (-m|--mount)" << std::endl;
            exit(1);
        }
        result["mount"] = vm["mount"].as<std::string>();
        result["debug"] = debug ? "true" : "false";

        if (!vm.count("name")) {
            std::cout << "Warning: Didn't specify fs name (-n|--name): using default: main" << std::endl;
            result["name"] = "main";
        } else {
            result["name"] = vm["name"].as<std::string>();
        }
    }
    catch(std::exception &e) {
        std::cerr << "Error: Options error: " << e.what() << std::endl;
        exit(1);
    }
    return result;
}