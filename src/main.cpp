#include <iostream>
#include <string>
#include <string_view>

#include "lb/util.h"
#include "vmlua/driver.h"

class cli_options
{
private:
    std::string _input_file;
    std::string _cli_program_name{"vmlua"};

public:
    explicit cli_options() {}

    // parse the command line arguments
    bool parse(int argc, char const *argv[]) noexcept
    {
        if (argc != 2)
        {
            return false;
        }
        _cli_program_name = argv[0];
        _input_file = argv[1];
        return true;
    }

    // check if the input file is readable
    bool valid() const
    {
        if (_input_file.empty())
        {
            return false;
        }
        if (!lb::file_util::is_readable(_input_file))
        {
            return false;
        }
        return true;
    }

    // get usage string
    std::string usage() const
    {
        return lb::string_util::concat(
            "Usage: ", _cli_program_name, " <input_file>");
    }

    std::string input_file() const noexcept { return _input_file; }
};

int main(int argc, char const *argv[])
{
    cli_options options;
    if (!options.parse(argc, argv))
    {
        std::cout << "No arguments" << std::endl;
        std::cout << options.usage() << std::endl;
        return 1;
    }
    if (!options.valid())
    {
        std::cout << "Invalid arguments, check if file exists" << std::endl;
        std::cout << options.usage() << std::endl;
        return 1;
    }
    lb::vmlua::driver driver(options.input_file());
    driver.run();
    return 0;
}
