-- AutoEval: Autonomous Driving Evaluation Framework
set_project("autoeval")
    set_version("0.1.0")
    set_languages("c++20")

    -- Configuration modes
    if is_mode("release") then
        set_optimize("fastest")
        set_symbols("hidden")
    elseif is_mode("debug") then
        set_symbols("debug")
        add_defines("AUTOEVAL_DEBUG")
    end

    -- Include directories
    add_includedirs("include", {public = true})

    -- Use vcpkg for dependencies
    add_requires("spdlog", "yaml-cpp", "nlohmann_json", "fmt", {configs = {shared = false}})

    -- ========================================
    -- Target: autoeval_core (Static Library)
    -- ========================================
    target("autoeval_core")
        set_kind("static")
        add_files("src/core/*.cpp", "src/metrics/*.cpp",
                  "src/loader/*.cpp", "src/plugin/*.cpp",
                  "src/report/*.cpp")
        add_headerfiles("include/(autoeval/**/*.h)")
        add_defines("AUTOEVAL_CORE_BUILD", "YAML_CPP_STATIC_DEFINE")
        add_packages("spdlog", "yaml-cpp", "nlohmann_json", "fmt")

        -- Platform specific settings
        if is_plat("windows") then
            add_defines("AUTOEVAL_WINDOWS", "_CRT_SECURE_NO_WARNINGS")
        elseif is_plat("linux") then
            add_defines("AUTOEVAL_LINUX")
            add_syslinks("pthread", "dl")
        end

    -- ========================================
    -- Target: autoeval_cli (Command Line Tool)
    -- ========================================
    target("autoeval_cli")
        set_kind("binary")
        add_files("cli/main.cpp")
        add_deps("autoeval_core")
        add_defines("AUTOEVAL_CORE_BUILD")
        add_packages("spdlog", "yaml-cpp", "nlohmann_json", "fmt")

    -- ========================================
    -- Target: autoeval_test (Test Suite - runs all tests)
    -- ========================================
    target("autoeval_test")
        set_kind("binary")
        add_deps("autoeval_test_types", "autoeval_test_metrics",
                 "autoeval_test_loader", "autoeval_test_plugin",
                 "autoeval_test_integration")
        on_run(function (target)
            import("lib.detect.find_program")
            local ok = os.exec("xmake run autoeval_test_types") == 0
            ok = os.exec("xmake run autoeval_test_metrics") == 0 and ok
            ok = os.exec("xmake run autoeval_test_loader") == 0 and ok
            ok = os.exec("xmake run autoeval_test_plugin") == 0 and ok
            ok = os.exec("xmake run autoeval_test_integration") == 0 and ok
            if not ok then osraise("Some tests failed") end
        end)

    -- Unit tests
    target("autoeval_test_types")
        set_kind("binary")
        add_files("tests/unit/test_types.cpp")
        add_deps("autoeval_core")
        add_packages("spdlog", "yaml-cpp", "nlohmann_json")

    target("autoeval_test_metrics")
        set_kind("binary")
        add_files("tests/unit/test_metrics.cpp")
        add_deps("autoeval_core")
        add_packages("spdlog", "yaml-cpp", "nlohmann_json")

    target("autoeval_test_loader")
        set_kind("binary")
        add_files("tests/unit/test_loader.cpp")
        add_deps("autoeval_core")
        add_packages("spdlog", "yaml-cpp", "nlohmann_json")

    target("autoeval_test_plugin")
        set_kind("binary")
        add_files("tests/unit/test_plugin.cpp")
        add_deps("autoeval_core")
        add_packages("spdlog", "yaml-cpp", "nlohmann_json")

    target("autoeval_test_integration")
        set_kind("binary")
        add_files("tests/integration/test_evaluator.cpp")
        add_deps("autoeval_core")
        add_packages("spdlog", "yaml-cpp", "nlohmann_json")

    target("autoeval_bench")
        set_kind("binary")
        add_files("tests/integration/test_benchmark.cpp")
        add_deps("autoeval_core")
        add_packages("spdlog", "yaml-cpp", "nlohmann_json")

    -- ========================================
    -- Task: clean-all
    -- ========================================
    task("clean-all")
        on_run(function ()
            import("core.base.option")
            import("core.project.task")
            task.run("clean")
            os.rm("$(buildir)")
            os.rm("$(projectdir)/.xmake")
        end)
        set_menu {
            usage = "xmake clean-all",
            description = "Remove all build artifacts"
        }
