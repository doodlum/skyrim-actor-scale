
#include <SimpleIni.h>


float fDeviation = 0.2f;
float fMaxScale = 1.2f;
float fMinScale = 0.8f;

#define GetSettingInt(a_section, a_setting, a_default) a_setting = (int)ini.GetLongValue(a_section, #a_setting, a_default);
#define SetSettingInt(a_section, a_setting) ini.SetLongValue(a_section, #a_setting, a_setting);

#define GetSettingFloat(a_section, a_setting, a_default) a_setting = (float)ini.GetDoubleValue(a_section, #a_setting, a_default);
#define SetSettingFloat(a_section, a_setting) ini.SetDoubleValue(a_section, #a_setting, a_setting);

#define GetSettingBool(a_section, a_setting, a_default) a_setting = ini.GetBoolValue(a_section, #a_setting, a_default);
#define SetSettingBool(a_section, a_setting) ini.SetBoolValue(a_section, #a_setting, a_setting);

void LoadINI()
{
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.LoadFile(L"Data\\SKSE\\Plugins\\DynamicActorScale.ini");
	GetSettingFloat("Settings", fDeviation, 0.1f);
	GetSettingFloat("Settings", fMaxScale, 1.1f);
	GetSettingFloat("Settings", fMinScale, 0.9f);
}

RE::FormID GetLocalFormIDIfApplicable(RE::TESForm* a_form)
{
	return a_form->GetFile(0) ? a_form->GetLocalFormID() : a_form->GetFormID();
}

void TESObjectREFR_SetScale(RE::TESObjectREFR* a_ref, float a_scale)
{
	using func_t = decltype(&TESObjectREFR_SetScale);
	REL::Relocation<func_t> func{ REL::RelocationID(19239, 19665) };
	func(a_ref, a_scale);
}

float GenerateSeededDistribution(RE::TESObjectREFR* a_ref) 
{
	std::normal_distribution<float> distribution{ 1, fDeviation };
	std::mt19937                    gen{ GetLocalFormIDIfApplicable(a_ref) };
	return std::clamp(distribution(gen), fMinScale, fMaxScale);
}

struct Hooks
	{
	struct Character_Update
	{
		static void thunk(RE::Character* a_actor, float a_delta)
		{
			func(a_actor, a_delta);
			// Guarantee the reference is fully loaded 
			if (a_actor->GetActorBase()->IsUnique() || a_actor->GetScale() != a_actor->GetActorBase()->GetHeight() || !a_actor->GetActorRuntimeData().currentProcess->InHighProcess() || !a_actor->GetCurrent3D()) {
				return;
			}
			logger::debug("Actor: {} {:X}", a_actor->GetName(), GetLocalFormIDIfApplicable(a_actor));
			auto oldScale = a_actor->GetScale();
			logger::debug("Old scale {}", oldScale);
			auto multiplier = GenerateSeededDistribution(a_actor);
			auto newScale = oldScale * multiplier;
			logger::debug("New scale {}", newScale);
			TESObjectREFR_SetScale(a_actor, newScale);
			a_actor->RemoveChange(16); // Remove the AddChange(16) from TESObjectREFR_SetScale so that scale change is not serialized
		} 
		static inline REL::Relocation<decltype(thunk)> func;
	};


	static void Install()
	{
		logger::info("Installing hook");
		stl::write_vfunc<RE::Character, 0xAD, Character_Update>();  // All other hooks were unreliable
	}
};



void Init()
{
	Hooks::Install();
	LoadINI();
}

void InitializeLog()
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		util::report_and_fail("Failed to find standard logging directory"sv);
	}

	*path /= std::format("{}.log"sv, Plugin::NAME);
	auto       sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

#ifndef NDEBUG
	const auto level = spdlog::level::trace;
#else
	const auto level = spdlog::level::info;
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
	log->set_level(level);
	log->flush_on(level);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%l] %v"s);
}

EXTERN_C [[maybe_unused]] __declspec(dllexport) bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
#ifndef NDEBUG
	while (!IsDebuggerPresent()) {};
#endif

	InitializeLog();

	logger::info("Loaded plugin");

	SKSE::Init(a_skse);

	Init();

	return true;
}

EXTERN_C [[maybe_unused]] __declspec(dllexport) constinit auto SKSEPlugin_Version = []() noexcept {
	SKSE::PluginVersionData v;
	v.PluginName("PluginName");
	v.PluginVersion({ 1, 0, 0, 0 });
	v.UsesAddressLibrary(true);
	v.HasNoStructUse();
	return v;
}();

EXTERN_C [[maybe_unused]] __declspec(dllexport) bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface*, SKSE::PluginInfo* pluginInfo)
{
	pluginInfo->name = SKSEPlugin_Version.pluginName;
	pluginInfo->infoVersion = SKSE::PluginInfo::kVersion;
	pluginInfo->version = SKSEPlugin_Version.pluginVersion;
	return true;
}
