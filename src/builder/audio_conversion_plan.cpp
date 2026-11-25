#include "rb4pc/builder/audio_conversion_plan.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
namespace rb4pc::builder { namespace {
std::string lower(std::string value) { std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){return static_cast<char>(std::tolower(c));}); return value; }
bool candidate(const std::filesystem::path& path) { const auto n=lower(path.filename().string()); return n.find(".mogg")!=std::string::npos||n.ends_with(".ogg")||n.find(".wav")!=std::string::npos||n.find(".wave")!=std::string::npos||n.find(".fev")!=std::string::npos||n.find(".vgs")!=std::string::npos; }
std::uint32_t u32(const unsigned char* b){return static_cast<std::uint32_t>(b[0])|(static_cast<std::uint32_t>(b[1])<<8)|(static_cast<std::uint32_t>(b[2])<<16)|(static_cast<std::uint32_t>(b[3])<<24);}
std::string escape(std::string_view v){std::ostringstream o;for(unsigned char c:v){if(c=='"')o<<"\\\"";else if(c=='\\')o<<"\\\\";else o<<static_cast<char>(c);}return o.str();}
const char* state_name(AudioPlanState s){switch(s){case AudioPlanState::runtime_ready:return "runtimeReady";case AudioPlanState::extraction_ready:return "extractionReady";case AudioPlanState::research_only:return "researchOnly";case AudioPlanState::invalid:return "invalid";}return "invalid";}
void count(AudioConversionPlan& p,AudioPlanState s){if(s==AudioPlanState::runtime_ready)++p.runtime_ready;else if(s==AudioPlanState::extraction_ready)++p.extraction_ready;else if(s==AudioPlanState::research_only)++p.research_only;else ++p.invalid;}
} // namespace
AudioConversionPlan plan_audio_conversions(const std::filesystem::path& root){
 if(!std::filesystem::is_directory(root))throw std::runtime_error("audio plan root is not a directory"); AudioConversionPlan plan;plan.root=root;
 for(const auto& item:std::filesystem::recursive_directory_iterator(root)){if(!item.is_regular_file()||!candidate(item.path()))continue;AudioPlanEntry e;e.relative_path=std::filesystem::relative(item.path(),root);e.size=item.file_size();
  try{std::ifstream in(item.path(),std::ios::binary);std::array<unsigned char,16>b{};in.read(reinterpret_cast<char*>(b.data()),b.size());const auto got=in.gcount();if(got<4)throw std::runtime_error("audio candidate is truncated");
   if(std::equal(b.begin(),b.begin()+4,reinterpret_cast<const unsigned char*>("OggS"))){e.state=AudioPlanState::runtime_ready;e.kind="Ogg";e.reason="native Ogg stream is runtime-readable";}
   else if(got>=12&&std::equal(b.begin(),b.begin()+4,reinterpret_cast<const unsigned char*>("RIFF"))){e.kind=std::string(reinterpret_cast<char*>(b.data()+8),4);if(e.kind=="WAVE"){e.state=AudioPlanState::runtime_ready;e.reason="RIFF/WAVE is runtime-readable";}else{e.state=AudioPlanState::research_only;e.reason="RIFF form requires format-specific validation";}}
   else if(got>=8){e.container_version=u32(b.data());const auto offset=u32(b.data()+4);bool embedded=false;if(offset+4<=e.size){in.clear();in.seekg(offset);std::array<char,4>m{};in.read(m.data(),4);embedded=in&&std::string(m.data(),4)=="OggS";}if(embedded){e.state=AudioPlanState::extraction_ready;e.kind="MOGG";e.reason="embedded Ogg stream can be extracted and verified";}else{e.state=AudioPlanState::research_only;e.kind="unknown";e.reason="no verified writable audio path for this signature";}}
   else throw std::runtime_error("audio candidate is truncated");
  }catch(const std::exception& ex){e.state=AudioPlanState::invalid;e.kind="invalid";e.reason=ex.what();}count(plan,e.state);plan.entries.push_back(std::move(e));}
 std::sort(plan.entries.begin(),plan.entries.end(),[](const auto&a,const auto&b){return a.relative_path.generic_string()<b.relative_path.generic_string();});return plan;}
std::string format_json(const AudioConversionPlan& p){std::ostringstream o;o<<"{\n  \"schemaVersion\": 1,\n  \"candidates\": "<<p.entries.size()<<",\n  \"runtimeReady\": "<<p.runtime_ready<<",\n  \"extractionReady\": "<<p.extraction_ready<<",\n  \"researchOnly\": "<<p.research_only<<",\n  \"invalid\": "<<p.invalid<<",\n  \"entries\": [";for(std::size_t i=0;i<p.entries.size();++i){const auto&e=p.entries[i];if(i)o<<',';o<<"\n    {\"path\": \""<<escape(e.relative_path.generic_string())<<"\", \"state\": \""<<state_name(e.state)<<"\", \"kind\": \""<<escape(e.kind)<<"\", \"reason\": \""<<escape(e.reason)<<"\", \"containerVersion\": "<<e.container_version<<", \"size\": "<<e.size<<'}';}o<<"\n  ]\n}\n";return o.str();}
} // namespace rb4pc::builder