module;

export module aengine.scripting.compiler;

import <cstdlib>;
import <filesystem>;
import <iostream>;
import <string>;
import <vector>;

export namespace almondnamespace::compiler 
{

    export bool compile_script_to_dll(const std::filesystem::path& input, const std::filesystem::path& output) {
        std::vector<std::string> clangArgs = {
            "clang++",
            "-std=c++20",
            "-shared",
            input.string(),
            "-o", output.string(),
            "-Iinclude",
            "-fno-rtti",
            "-fno-exceptions",
            "-O2"
        };

        std::string cmd;
        for (const auto& arg : clangArgs) {
            cmd += arg + " ";
        }

        std::cout << "[compiler] running: " << cmd << std::endl;
        int result = std::system(cmd.c_str());
        if (result != 0) {
            std::cerr << "[compiler] clang++ failed with code: " << result << std::endl;
            return false;
        }

        return true;
    }

}
