-- Example plugin build config for AutoEval
-- Build from project root:
--   cd plugins && xmake f -p mingw -m release && xmake build
--
-- Or from root:
--   xmake f -p mingw -m release && xmake build plugins/example_metric

add_deps("autoeval_core")

if is_plat("windows") then
    set_kind("shared")
    add_defines("AUTOEVAL_WINDOWS", "AUTOEVAL_PLUGIN_BUILD")
    add_files("src/hard_brake_metric.cpp")
    set_target("example_metric", "AutoEvalHardBrake")
    add_files("src/binary_loader.cpp")
    set_target("example_loader", "AutoEvalBinary")

elseif is_plat("linux") then
    set_kind("shared")
    add_defines("AUTOEVAL_LINUX", "AUTOEVAL_PLUGIN_BUILD")
    add_files("src/hard_brake_metric.cpp")
    set_target("example_metric", "autoeval_hard_brake")
    add_files("src/binary_loader.cpp")
    set_target("example_loader", "autoeval_binary")
    add_syslinks("pthread", "dl")

elseif is_plat("macos") then
    set_kind("shared")
    add_defines("AUTOEVAL_MACOS", "AUTOEVAL_PLUGIN_BUILD")
    add_files("src/hard_brake_metric.cpp")
    set_target("example_metric", "autoeval_hard_brake")
    add_files("src/binary_loader.cpp")
    set_target("example_loader", "autoeval_binary")
    add_syslinks("pthread", "dl")
end
