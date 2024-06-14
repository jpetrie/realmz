#include <algorithm>
#include <deque>
#include <filesystem>
#include <memory>

#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>
#include <resource_file/IndexFormats/Formats.hh>
#include <resource_file/ResourceFile.hh>
#include <resource_file/ResourceTypes.hh>
#include <resource_file/QuickDrawFormats.hh>
#include "ResourceManager.h"

 ResourceManager_Rect rect_from_data(StringReader &data) {
	ResourceManager_Rect r;
	r.top = data.get_u16b();
	r.left = data.get_u16b();
	r.bottom = data.get_u16b();
	r.right = data.get_u16b();
	return r;
 }
 
static int16_t resError = noErr;

class ResourceManager {
public:
	ResourceManager() {}

	void use(ResourceFile rf);
	std::shared_ptr<const ResourceFile::Resource> find_resource(int32_t type, int16_t id);
	std::shared_ptr<const ResourceFile> get_current_res_file();

private:
	std::deque<ResourceFile> resFiles;
	std::shared_ptr<const ResourceFile> curResFile;
};

void ResourceManager::use(ResourceFile rf) {
	this->resFiles.push_front(rf);
	this->curResFile = std::make_shared<const ResourceFile>(rf);
}

std::shared_ptr<const ResourceFile> ResourceManager::get_current_res_file() {
	return this->curResFile;
}

std::shared_ptr<const ResourceFile::Resource> ResourceManager::find_resource(int32_t type, int16_t id) {
	if (this->curResFile->resource_exists(type, id)) return this->curResFile->get_resource(type, id);
	// std::for_each(this->resFiles.begin(), this->resFiles.end(), [type, id](const ResourceFile &rf) {
	// 	if (rf.resource_exists(type, id))
	// 		return rf.get_resource(type, id);
	// });
	return NULL;
}

static ResourceManager rm;

void *ResourceManager_GetResource(int32_t theType, int16_t theID) {
	auto resource = rm.find_resource(theType, theID);

	resError = resNotFound;
	return NULL;
}
struct PixelPatternResourceHeader {
  be_uint16_t type;
  be_uint32_t pixel_map_offset;
  be_uint32_t pixel_data_offset;
  be_uint32_t unused1; // TMPL: "Expanded pixel image" (probably ptr to decompressed data when used by QuickDraw)
  be_uint16_t unused2; // TMPL: "Pattern valid flag" (unused in stored resource)
  be_uint32_t reserved; // TMPL: "Expanded pattern"
  uint8_t monochrome_pattern[8];
} __attribute__((packed));

ResourceManager_PixPat ResourceManager_get_ppat_resource(uint16_t patID) {
	auto resource = rm.find_resource(RESOURCE_TYPE_ppat, patID);
	StringReader r(resource->data.data(), resource->data.size());
	const auto& header = r.get<PixelPatternResourceHeader>();
	const auto& pixmap_header = r.pget<PixelMapHeader>(header.pixel_map_offset + 4);
	ResourceManager_PixMap rmPixMap = {
		pixmap_header.pixel_size,
		{
			pixmap_header.bounds.y1,
			pixmap_header.bounds.x1,
			pixmap_header.bounds.y2,
			pixmap_header.bounds.x2
		}
	};

	ResourceFile::DecodedPattern pattern = ResourceFile::decode_ppat(resource);
	const char *patData = reinterpret_cast<const char*>(pattern.pattern.get_data());

	return {
		header.type,
		rmPixMap,
		&patData
	};
}

ResourceManager_Picture ResourceManager_get_pict_resource(uint16_t picID) {
	auto resource = rm.find_resource(RESOURCE_TYPE_PICT, picID);
	ResourceFile::DecodedPictResource p = rm.get_current_res_file()->decode_PICT(resource);

	// Have to copy the raw data out of the Image object, so that it doesn't get freed
	// out from under us
	void *data = malloc(p.image.get_data_size());
	memcpy(data, p.image.get_data(), p.image.get_data_size());

	return {
		0,
		{ 0, 0, (int32_t)p.image.get_height(), (int32_t)p.image.get_width() },
		data
	};
}

ResourceManager_Window ResourceManager_get_wind_resource(int16_t windowID) {
	auto resource = rm.find_resource(RESOURCE_TYPE_WIND, windowID);
    StringReader data(resource->data.data(), resource->data.size());

	ResourceManager_Rect r = rect_from_data(data);
	uint16_t procID = data.get_u16b();
	bool visible = (bool)data.get_u16b();
	bool dismissable = (bool)data.get_u16b();
	uint32_t refCon = data.get_u32b();
	uint8_t nameSize = data.get_u8();
	std::string n = data.read(nameSize);

	ResourceManager_Window w = {
		r,
		procID,
		visible,
		dismissable,
		refCon
	};
	w.windowTitle[0] = nameSize;
	strcpy(&w.windowTitle[1], n.c_str());
	return w;
}

ResourceManager_Dialog ResourceManager_get_dlog_resource(int16_t dialogID) {
	auto dlog = rm.find_resource(RESOURCE_TYPE_DLOG, dialogID);
	StringReader data(dlog->data.data(), dlog->data.size());

	ResourceManager_Rect r = rect_from_data(data);
	uint16_t wDefID = data.get_u16b();
	bool visible = (bool)data.get_u8();
	data.read(1);
	bool dismissable = (bool)data.get_u8();
	data.read(1);
	uint32_t refCon = data.get_u32b();
	int16_t ditlID = data.get_s16b();

	return {
		r,
		wDefID,
		visible,
		dismissable,
		refCon,
		ditlID
	};
}

uint16_t ResourceManager_get_ditl_resources(int16_t ditlID, ResourceManager_DialogItem **items) {
	auto ditl = rm.find_resource(RESOURCE_TYPE_DITL, ditlID);
	StringReader ditlData = StringReader(ditl->data.data(), ditl->data.size());
	uint16_t numItems = ditlData.get_u16b() + 1;
	*items = (ResourceManager_DialogItem *)malloc(sizeof(ResourceManager_DialogItem) * numItems);
	for (int i = 0; i < numItems; i++) {
		ditlData.read(4);
		ResourceManager_Rect dispWindow = rect_from_data(ditlData);
		uint8_t type = ditlData.get_u8();
		bool enabled = (bool)(type & 0x80);
		type = (type & 0x7F);
		switch (type) {
			// PICT Resource
			case 64: {
				ditlData.read(1);
				uint16_t pictID = ditlData.get_u16b();
				auto p = ResourceManager_get_pict_resource(pictID);
				(*items)[i].type = (*items)[i].DIALOG_ITEM_TYPE_PICT;
				(*items)[i].dialogItem.pict.dispRect = dispWindow;
				(*items)[i].dialogItem.pict.enabled = enabled;
				(*items)[i].dialogItem.pict.p = p;
				break;
			}
			default:
				break;
		}
	}
	return numItems;
}

ResourceManager_Sound ResourceManager_get_snd_resource(int16_t soundID) {
	auto snd = rm.find_resource(RESOURCE_TYPE_snd, soundID);
	ResourceFile::DecodedSoundResource soundData = rm.get_current_res_file()->decode_snd(snd);
	size_t len = soundData.data.size();
	void *data = malloc(len);
	memcpy(data, soundData.data.data(), len);
	return ResourceManager_Sound {
		.freq = soundData.sample_rate,
		.data = data,
		.len = len
	};
}

int16_t ResourceManager_OpenResFile(const char filename[256], signed char permission) {
	auto fn = std::string(filename);
	std::replace(fn.begin(), fn.end(), ':', '/');
	auto path = std::string("/Users/applegatedt/workspace/realmz/deployment/Realmz");
	path.append(fn);
	path.append("/..namedfork/rsrc");
	if (!std::filesystem::exists(path)) {
		return -1;
	}
	auto f = load_file(path);
	ResourceFile rf = parse_resource_fork(f);
	rm.use(rf);
	return 0;
}