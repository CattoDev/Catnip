#include <Geode/Geode.hpp>

using namespace geode::prelude;

/*// first time config
$on_mod(Loaded) {
    auto mod = Mod::get();
    int val = mod->getSettingValue<int64_t>("max-threads");
    
    if(val == -1) {
        mod->setSettingValue<int64_t>("max-threads", std::thread::hardware_concurrency());
    }
}*/