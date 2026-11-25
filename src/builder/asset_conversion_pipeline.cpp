#include "rb4pc/builder/asset_conversion_pipeline.hpp"
#include "rb4pc/builder/audio_conversion_plan.hpp"
#include "rb4pc/builder/texture_conversion_plan.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace rb4pc::builder {
std::string format_json(const AssetConversionPipelineResult& r){std::ostringstream o;o<<"{\n  \"schemaVersion\": 1,\n  \"convertedTextures\": "<<r.converted_textures<<",\n  \"extractedAudio\": "<<r.extracted_audio<<",\n  \"skippedTextureReadOnly\": "<<r.skipped_texture_read_only<<",\n  \"skippedTextureInvalid\": "<<r.skipped_texture_invalid<<",\n  \"skippedAudioRuntimeReady\": "<<r.skipped_audio_runtime_ready<<",\n  \"skippedAudioResearchOnly\": "<<r.skipped_audio_research_only<<",\n  \"skippedAudioInvalid\": "<<r.skipped_audio_invalid<<"\n}\n";return o.str();}
AssetConversionPipelineResult convert_asset_pipeline(const std::filesystem::path& source_root,const std::filesystem::path& destination_root){
 if(!std::filesystem::is_directory(source_root))throw std::runtime_error("asset pipeline source is not a directory");
 if(destination_root.empty()||std::filesystem::exists(destination_root))throw std::runtime_error("asset pipeline destination is empty or already exists");
 const auto parent=destination_root.has_parent_path()?destination_root.parent_path():std::filesystem::path{"."};if(!std::filesystem::is_directory(parent))throw std::runtime_error("asset pipeline destination parent does not exist");
 auto temporary=destination_root;temporary+=".rb4pc-part";if(std::filesystem::exists(temporary))throw std::runtime_error("asset pipeline temporary destination exists");
 AssetConversionPipelineResult result;result.directory=destination_root;result.summary=destination_root/"asset-pipeline-summary.json";
 try{std::filesystem::create_directory(temporary);const auto textures=convert_texture_batch(source_root,temporary/"t");std::filesystem::rename(temporary/"t",temporary/"textures");const auto audio=extract_audio_batch(source_root,temporary/"a");std::filesystem::rename(temporary/"a",temporary/"audio");
  result.converted_textures=textures.items.size();result.extracted_audio=audio.items.size();result.skipped_texture_read_only=textures.skipped_read_only;result.skipped_texture_invalid=textures.skipped_invalid;result.skipped_audio_runtime_ready=audio.skipped_runtime_ready;result.skipped_audio_research_only=audio.skipped_research_only;result.skipped_audio_invalid=audio.skipped_invalid;
  std::ofstream out(temporary/"asset-pipeline-summary.json",std::ios::binary|std::ios::trunc);out<<format_json(result);out.close();if(!out)throw std::runtime_error("could not write asset pipeline summary");std::filesystem::rename(temporary,destination_root);
 }catch(...){std::error_code ignored;std::filesystem::remove_all(temporary,ignored);throw;}return result;
}
} // namespace rb4pc::builder