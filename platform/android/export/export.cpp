#include "version.h"
#include "export.h"
#include "tools/editor/editor_settings.h"
#include "tools/editor/editor_import_export.h"
#include "tools/editor/editor_node.h"
#include "io/zip_io.h"
#include "io/marshalls.h"
#include "globals.h"
#include "os/file_access.h"
#include "os/os.h"
#include "platform/android/logo.h"

class EditorExportPlatformAndroid : public EditorExportPlatform {

	OBJ_TYPE( EditorExportPlatformAndroid,EditorExportPlatform );

	String custom_release_package;
	String custom_debug_package;

	int version_code;
	String version_name;
	String package;
	String name;
	String icon;
	bool _signed;
	int orientation;

	String release_keystore;
	String release_username;

	struct APKExportData {

		zipFile apk;
		EditorProgress *ep;
	};

	struct Device {

		String id;
		String name;
		String description;
	};

	Vector<Device> devices;
	bool devices_changed;
	Mutex *device_lock;
	Thread *device_thread;
	Ref<ImageTexture> logo;

	volatile bool quit_request;


	static void _device_poll_thread(void *ud);

	String get_project_name() const;
	void _fix_manifest(Vector<uint8_t>& p_manifest);
	void _fix_resources(Vector<uint8_t>& p_manifest);
	static Error save_apk_file(void *p_userdata,const String& p_path, const Vector<uint8_t>& p_data,int p_file,int p_total);

protected:

	bool _set(const StringName& p_name, const Variant& p_value);
	bool _get(const StringName& p_name,Variant &r_ret) const;
	void _get_property_list( List<PropertyInfo> *p_list) const;

public:

	virtual String get_name() const { return "Android"; }
	virtual ImageCompression get_image_compression() const { return IMAGE_COMPRESSION_ETC1; }
	virtual Ref<Texture> get_logo() const { return logo; }


	virtual bool poll_devices();
	virtual int get_device_count() const;
	virtual String get_device_name(int p_device) const;
	virtual String get_device_info(int p_device) const;
	virtual Error run(int p_device);

	virtual bool requieres_password(bool p_debug) const { return !p_debug; }
	virtual String get_binary_extension() const { return "apk"; }
	virtual Error export_project(const String& p_path,bool p_debug,const String& p_password="");

	virtual bool can_export(String *r_error=NULL) const;

	EditorExportPlatformAndroid();
	~EditorExportPlatformAndroid();
};

bool EditorExportPlatformAndroid::_set(const StringName& p_name, const Variant& p_value) {

	String n=p_name;

	if (n=="version/code")
		version_code=p_value;
	else if (n=="version/name")
		version_name=p_value;
	else if (n=="package/unique_name")
		package=p_value;
	else if (n=="package/name")
		name=p_value;
	else if (n=="package/icon")
		icon=p_value;
	else if (n=="package/signed")
		_signed=p_value;
	else if (n=="screen/orientation")
		orientation=p_value;
	else if (n=="keystore/release")
		release_keystore=p_value;
	else if (n=="keystore/release_user")
		release_username=p_value;
	else
		return false;

	return true;
}

bool EditorExportPlatformAndroid::_get(const StringName& p_name,Variant &r_ret) const{

	String n=p_name;

	if (n=="version/code")
		r_ret=version_code;
	else if (n=="version/name")
		r_ret=version_name;
	else if (n=="package/unique_name")
		r_ret=package;
	else if (n=="package/name")
		r_ret=name;
	else if (n=="package/icon")
		r_ret=icon;
	else if (n=="package/signed")
		r_ret=_signed;
	else if (n=="screen/orientation")
		r_ret=orientation;
	else if (n=="keystore/release")
		r_ret=release_keystore;
	else if (n=="keystore/release_user")
		r_ret=release_username;
	else
		return false;

	return true;
}
void EditorExportPlatformAndroid::_get_property_list( List<PropertyInfo> *p_list) const{

	p_list->push_back( PropertyInfo( Variant::STRING, "custom_package/debug", PROPERTY_HINT_FILE,"apk"));
	p_list->push_back( PropertyInfo( Variant::STRING, "custom_package/release", PROPERTY_HINT_FILE,"apk"));
	p_list->push_back( PropertyInfo( Variant::INT, "version/code", PROPERTY_HINT_RANGE,"1,65535,1"));
	p_list->push_back( PropertyInfo( Variant::STRING, "version/name") );
	p_list->push_back( PropertyInfo( Variant::STRING, "package/unique_name") );
	p_list->push_back( PropertyInfo( Variant::STRING, "package/name") );
	p_list->push_back( PropertyInfo( Variant::STRING, "package/icon",PROPERTY_HINT_FILE,"png") );
	p_list->push_back( PropertyInfo( Variant::BOOL, "package/signed") );
	p_list->push_back( PropertyInfo( Variant::INT, "screen/orientation",PROPERTY_HINT_ENUM,"Landscape,Portrait") );
	p_list->push_back( PropertyInfo( Variant::STRING, "keystore/release",PROPERTY_HINT_FILE,"keystore") );
	p_list->push_back( PropertyInfo( Variant::STRING, "keystore/release_user" ) );

	//p_list->push_back( PropertyInfo( Variant::INT, "resources/pack_mode", PROPERTY_HINT_ENUM,"Copy,Single Exec.,Pack (.pck),Bundles (Optical)"));

}


static String _parse_string(const uint8_t *p_bytes,bool p_utf8) {

	uint32_t offset=0;
	uint32_t len = decode_uint16(&p_bytes[offset]);

	if (p_utf8) {
		//don't know how to read extended utf8, this will have to be for now
		len>>=8;

	}
	offset+=2;
	printf("len %i, unicode: %i\n",len,int(p_utf8));

	if (p_utf8) {

		Vector<uint8_t> str8;
		str8.resize(len+1);
		for(int i=0;i<len;i++) {
			str8[i]=p_bytes[offset+i];
		}
		str8[len]=0;
		String str;
		str.parse_utf8((const char*)str8.ptr());
		return str;
	} else {

		String str;
		for(int i=0;i<len;i++) {
			CharType c = decode_uint16(&p_bytes[offset+i*2]);
			if (c==0)
				break;
			str += String::chr(c);
		}
		return str;
	}

}

void EditorExportPlatformAndroid::_fix_resources(Vector<uint8_t>& p_manifest) {


	const int UTF8_FLAG = 0x00000100;
	print_line("*******************GORRRGLE***********************");

	uint32_t header = decode_uint32(&p_manifest[0]);
	uint32_t filesize = decode_uint32(&p_manifest[4]);
	uint32_t string_block_len = decode_uint32(&p_manifest[16]);
	uint32_t string_count = decode_uint32(&p_manifest[20]);
	uint32_t string_flags = decode_uint32(&p_manifest[28]);
	const uint32_t string_table_begins = 40;

	Vector<String> string_table;

	printf("stirng block len: %i\n",string_block_len);
	printf("stirng count: %i\n",string_count);
	printf("flags: %x\n",string_flags);

	for(int i=0;i<string_count;i++) {

		uint32_t offset = decode_uint32(&p_manifest[string_table_begins+i*4]);
		offset+=string_table_begins+string_count*4;

		String str = _parse_string(&p_manifest[offset],string_flags&UTF8_FLAG);

		if (str.begins_with("godot-project-name")) {


			if (str=="godot-project-name") {
				//project name
				str = get_project_name();

			} else {

				String lang = str.substr(str.find_last("-")+1,str.length()).replace("-","_");
				String prop = "application/name_"+lang;
				if (Globals::get_singleton()->has(prop)) {
					str = Globals::get_singleton()->get(prop);
				} else {
					str = get_project_name();
				}
			}
		}

		string_table.push_back(str);

	}

	//write a new string table, but use 16 bits
	Vector<uint8_t> ret;
	ret.resize(string_table_begins+string_table.size()*4);

	for(int i=0;i<string_table_begins;i++) {

		ret[i]=p_manifest[i];
	}

	int ofs=0;
	for(int i=0;i<string_table.size();i++) {

		encode_uint32(ofs,&ret[string_table_begins+i*4]);
		ofs+=string_table[i].length()*2+2+2;
	}

	ret.resize(ret.size()+ofs);
	uint8_t *chars=&ret[ret.size()-ofs];
	for(int i=0;i<string_table.size();i++) {

		String s = string_table[i];
		encode_uint16(s.length(),chars);
		chars+=2;
		for(int j=0;j<s.length();j++) {
			encode_uint16(s[j],chars);
			chars+=2;
		}
		encode_uint16(0,chars);
		chars+=2;
	}

	//pad
	while(ret.size()%4)
		ret.push_back(0);

	//change flags to not use utf8
	encode_uint32(string_flags&~0x100,&ret[28]);
	//change length
	encode_uint32(ret.size()-12,&ret[16]);
	//append the rest...
	int rest_from = 12+string_block_len;
	int rest_to = ret.size();
	int rest_len = (p_manifest.size() - rest_from);
	ret.resize(ret.size() + (p_manifest.size() - rest_from) );
	for(int i=0;i<rest_len;i++) {
		ret[rest_to+i]=p_manifest[rest_from+i];
	}
	//finally update the size
	encode_uint32(ret.size(),&ret[4]);


	p_manifest=ret;
	printf("end\n");
}

String EditorExportPlatformAndroid::get_project_name() const {

	String aname;
	if (this->name!="") {
		aname=this->name;
	} else {
		aname = Globals::get_singleton()->get("application/name");

	}

	if (aname=="") {
		aname=_MKSTR(VERSION_NAME);
	}

	return aname;
}


void EditorExportPlatformAndroid::_fix_manifest(Vector<uint8_t>& p_manifest) {


	const int CHUNK_AXML_FILE = 0x00080003;
	const int CHUNK_RESOURCEIDS = 0x00080180;
	const int CHUNK_STRINGS = 0x001C0001;
	const int CHUNK_XML_END_NAMESPACE = 0x00100101;
	const int CHUNK_XML_END_TAG = 0x00100103;
	const int CHUNK_XML_START_NAMESPACE = 0x00100100;
	const int CHUNK_XML_START_TAG = 0x00100102;
	const int CHUNK_XML_TEXT = 0x00100104;
	const int UTF8_FLAG = 0x00000100;

	Vector<String> string_table;

	uint32_t ofs=0;


	uint32_t header = decode_uint32(&p_manifest[ofs]);
	uint32_t filesize = decode_uint32(&p_manifest[ofs+4]);
	ofs+=8;

//	print_line("FILESIZE: "+itos(filesize)+" ACTUAL: "+itos(p_manifest.size()));

	uint32_t string_count;
	uint32_t styles_count;
	uint32_t string_flags;
	uint32_t string_data_offset;

	uint32_t styles_offset;
	uint32_t string_table_begins;
	uint32_t string_table_ends;
	Vector<uint8_t> stable_extra;

	while(ofs < p_manifest.size()) {

		uint32_t chunk = decode_uint32(&p_manifest[ofs]);
		uint32_t size = decode_uint32(&p_manifest[ofs+4]);


		switch(chunk) {

			case CHUNK_STRINGS: {


				int iofs=ofs+8;

				uint32_t string_count=decode_uint32(&p_manifest[iofs]);
				uint32_t styles_count=decode_uint32(&p_manifest[iofs+4]);
				uint32_t string_flags=decode_uint32(&p_manifest[iofs+8]);
				uint32_t string_data_offset=decode_uint32(&p_manifest[iofs+12]);
				uint32_t styles_offset=decode_uint32(&p_manifest[iofs+16]);
/*
				printf("string count: %i\n",string_count);
				printf("flags: %i\n",string_flags);
				printf("sdata ofs: %i\n",string_data_offset);
				printf("styles ofs: %i\n",styles_offset);
*/
				uint32_t st_offset=iofs+20;
				string_table.resize(string_count);
				uint32_t string_end=0;

				string_table_begins=st_offset;


				for(int i=0;i<string_count;i++) {

					uint32_t string_at = decode_uint32(&p_manifest[st_offset+i*4]);
					string_at+=st_offset+string_count*4;

					ERR_EXPLAIN("Unimplemented, can't read utf8 string table.");
					ERR_FAIL_COND(string_flags&UTF8_FLAG);

					if (string_flags&UTF8_FLAG) {



					} else {
						uint32_t len = decode_uint16(&p_manifest[string_at]);
						Vector<CharType> ucstring;
						ucstring.resize(len+1);
						for(int j=0;j<len;j++) {
							uint16_t c=decode_uint16(&p_manifest[string_at+2+2*j]);
							ucstring[j]=c;
						}
						string_end=MAX(string_at+2+2*len,string_end);
						ucstring[len]=0;
						string_table[i]=ucstring.ptr();
					}


//					print_line("String "+itos(i)+": "+string_table[i]);
				}

				for(int i=string_end;i<(ofs+size);i++) {
					stable_extra.push_back(p_manifest[i]);
				}

//				printf("stable extra: %i\n",int(stable_extra.size()));
				string_table_ends=ofs+size;

//				print_line("STABLE SIZE: "+itos(size)+" ACTUAL: "+itos(string_table_ends));

			} break;
			case CHUNK_XML_START_TAG: {

				int iofs=ofs+8;
				uint32_t line=decode_uint32(&p_manifest[iofs]);
				uint32_t nspace=decode_uint32(&p_manifest[iofs+8]);
				uint32_t name=decode_uint32(&p_manifest[iofs+12]);
				uint32_t check=decode_uint32(&p_manifest[iofs+16]);

				String tname=string_table[name];

//				printf("NSPACE: %i\n",nspace);
				//printf("NAME: %i (%s)\n",name,tname.utf8().get_data());
				//printf("CHECK: %x\n",check);
				uint32_t attrcount=decode_uint32(&p_manifest[iofs+20]);
				iofs+=28;
				//printf("ATTRCOUNT: %x\n",attrcount);
				for(int i=0;i<attrcount;i++) {
					uint32_t attr_nspace=decode_uint32(&p_manifest[iofs]);
					uint32_t attr_name=decode_uint32(&p_manifest[iofs+4]);
					uint32_t attr_value=decode_uint32(&p_manifest[iofs+8]);
					uint32_t attr_flags=decode_uint32(&p_manifest[iofs+12]);
					uint32_t attr_resid=decode_uint32(&p_manifest[iofs+16]);


					String value;
					if (attr_value!=0xFFFFFFFF)
						value=string_table[attr_value];
					else
						value="Res #"+itos(attr_resid);
					String attrname = string_table[attr_name];
					String nspace;
					if (attr_nspace!=0xFFFFFFFF)
						nspace=string_table[attr_nspace];
					else
						nspace="";

					printf("ATTR %i NSPACE: %i\n",i,attr_nspace);
					printf("ATTR %i NAME: %i (%s)\n",i,attr_name,attrname.utf8().get_data());
					printf("ATTR %i VALUE: %i (%s)\n",i,attr_value,value.utf8().get_data());
					printf("ATTR %i FLAGS: %x\n",i,attr_flags);
					printf("ATTR %i RESID: %x\n",i,attr_resid);

					//replace project information
					if (tname=="manifest" && attrname=="package") {

						print_line("FOUND PACKAGE");
						string_table[attr_value]=package;
					}

					//print_line("tname: "+tname);
					//print_line("nspace: "+nspace);
					//print_line("attrname: "+attrname);
					if (tname=="manifest" && /*nspace=="android" &&*/ attrname=="versionCode") {

						print_line("FOUND versioncode");
						encode_uint32(version_code,&p_manifest[iofs+16]);
					}


					if (tname=="manifest" && /*nspace=="android" &&*/ attrname=="versionName") {

						print_line("FOUND versionname");
						if (attr_value==0xFFFFFFFF) {
							WARN_PRINT("Version name in a resource, should be plaintext")
						} else
							string_table[attr_value]=version_name;
					}

					if (tname=="activity" && /*nspace=="android" &&*/ attrname=="screenOrientation") {

						print_line("FOUND screen orientation");
						if (attr_value==0xFFFFFFFF) {
							WARN_PRINT("Version name in a resource, should be plaintext")
						} else {
							string_table[attr_value]=(orientation==0?"landscape":"portrait");
						}
					}

					if (tname=="application" && /*nspace=="android" &&*/ attrname=="label") {

						print_line("FOUND application");
						if (attr_value==0xFFFFFFFF) {
							WARN_PRINT("Application name in a resource, should be plaintext.")
						} else {

							String aname = get_project_name();
							string_table[attr_value]=aname;
						}
					}
					if (tname=="activity" && /*nspace=="android" &&*/ attrname=="label") {

						print_line("FOUND activity name");
						if (attr_value==0xFFFFFFFF) {
							WARN_PRINT("Activity name in a resource, should be plaintext")
						} else {
							String aname;
							if (this->name!="") {
								aname=this->name;
							} else {
								aname = Globals::get_singleton()->get("application/name");

							}

							if (aname=="") {
								aname=_MKSTR(VERSION_NAME);
							}

							print_line("APP NAME IS..."+aname);
							string_table[attr_value]=aname;
						}
					}

					iofs+=20;
				}

			} break;
		}
		printf("chunk %x: size: %d\n",chunk,size);

		ofs+=size;
	}

	printf("end\n");

	//create new andriodmanifest binary

	Vector<uint8_t> ret;
	ret.resize(string_table_begins+string_table.size()*4);

	for(int i=0;i<string_table_begins;i++) {

		ret[i]=p_manifest[i];
	}

	ofs=0;
	for(int i=0;i<string_table.size();i++) {

		encode_uint32(ofs,&ret[string_table_begins+i*4]);
		ofs+=string_table[i].length()*2+2+2;
		print_line("ofs: "+itos(i)+": "+itos(ofs));
	}
	ret.resize(ret.size()+ofs);
	uint8_t *chars=&ret[ret.size()-ofs];
	for(int i=0;i<string_table.size();i++) {

		String s = string_table[i];
		print_line("savint string :"+s);
		encode_uint16(s.length(),chars);
		chars+=2;
		for(int j=0;j<s.length();j++) { //include zero?
			encode_uint16(s[j],chars);
			chars+=2;
		}
		encode_uint16(0,chars);
		chars+=2;

	}


	ret.resize(ret.size()+stable_extra.size());
	while(ret.size()%4)
		ret.push_back(0);

	for(int i=0;i<stable_extra.size();i++) {

		chars[i]=stable_extra[i];
	}


	uint32_t new_stable_end=ret.size();

	uint32_t extra = (p_manifest.size()-string_table_ends);
	ret.resize(new_stable_end + extra);
	for(int i=0;i<extra;i++)
		ret[new_stable_end+i]=p_manifest[string_table_ends+i];

	while(ret.size()%4)
		ret.push_back(0);
	encode_uint32(ret.size(),&ret[4]); //update new file size

	encode_uint32(new_stable_end-8,&ret[12]); //update new string table size

	print_line("file size: "+itos(ret.size()));

	p_manifest=ret;






#if 0
	uint32_t header[9];
	for(int i=0;i<9;i++) {
		header[i]=decode_uint32(&p_manifest[i*4]);
	}

	print_line("STO: "+itos(header[3]));
	uint32_t st_offset=9*4;
	//ERR_FAIL_COND(header[3]!=0x24)
	uint32_t string_count=header[4];


	string_table.resize(string_count);

	for(int i=0;i<string_count;i++) {

		uint32_t string_at = decode_uint32(&p_manifest[st_offset+i*4]);
		string_at+=st_offset+string_count*4;
		uint32_t len = decode_uint16(&p_manifest[string_at]);
		Vector<CharType> ucstring;
		ucstring.resize(len+1);
		for(int j=0;j<len;j++) {
			uint16_t c=decode_uint16(&p_manifest[string_at+2+2*j]);
			ucstring[j]=c;
		}
		ucstring[len]=0;
		string_table[i]=ucstring.ptr();
	}


#endif

}



Error EditorExportPlatformAndroid::save_apk_file(void *p_userdata,const String& p_path, const Vector<uint8_t>& p_data,int p_file,int p_total) {

	APKExportData *ed=(APKExportData*)p_userdata;
	String dst_path=p_path;
	dst_path=dst_path.replace_first("res://","assets/");

	zipOpenNewFileInZip(ed->apk,
		dst_path.utf8().get_data(),
		NULL,
		NULL,
		0,
		NULL,
		0,
		NULL,
		Z_DEFLATED,
		Z_DEFAULT_COMPRESSION);


	zipWriteInFileInZip(ed->apk,p_data.ptr(),p_data.size());
	zipCloseFileInZip(ed->apk);
	ed->ep->step("File: "+p_path,3+p_file*100/p_total);
	return OK;

}



Error EditorExportPlatformAndroid::export_project(const String& p_path,bool p_debug,const String& p_password) {

	String src_apk;

	EditorProgress ep("export","Exporting for Android",104);

	String apk_path = EditorSettings::get_singleton()->get_settings_path()+"/templates/";

	if (p_debug) {

		src_apk=custom_debug_package!=""?custom_debug_package:apk_path+"android_debug.apk";
	} else {

		src_apk=custom_release_package!=""?custom_release_package:apk_path+"android_release.apk";

	}


	FileAccess *src_f=NULL;
	zlib_filefunc_def io = zipio_create_io_from_file(&src_f);



	ep.step("Creating APK",0);

	unzFile pkg = unzOpen2(src_apk.utf8().get_data(), &io);
	if (!pkg) {

		EditorNode::add_io_error("Could not find template APK to export:\n"+src_apk);
		return ERR_FILE_NOT_FOUND;
	}

	ERR_FAIL_COND_V(!pkg, ERR_CANT_OPEN);
	int ret = unzGoToFirstFile(pkg);

	zlib_filefunc_def io2=io;
	FileAccess *dst_f=NULL;
	io2.opaque=&dst_f;
	zipFile	apk=zipOpen2(p_path.utf8().get_data(),APPEND_STATUS_CREATE,NULL,&io2);


	while(ret==UNZ_OK) {

		//get filename
		unz_file_info info;
		char fname[16384];
		ret = unzGetCurrentFileInfo(pkg,&info,fname,16384,NULL,0,NULL,0);

		String file=fname;

		Vector<uint8_t> data;
		data.resize(info.uncompressed_size);

		//read
		unzOpenCurrentFile(pkg);
		unzReadCurrentFile(pkg,data.ptr(),data.size());
		unzCloseCurrentFile(pkg);

		//write

		if (file=="AndroidManifest.xml") {

			_fix_manifest(data);
		}

		if (file=="resources.arsc") {

			_fix_resources(data);
		}

		if (file=="res/drawable/icon.png") {
			bool found=false;

			if (this->icon!="" && this->icon.ends_with(".png")) {

				FileAccess *f = FileAccess::open(this->icon,FileAccess::READ);
				if (f) {

					data.resize(f->get_len());
					f->get_buffer(data.ptr(),data.size());
					memdelete(f);
					found=true;
				}

			}

			if (!found) {

				String appicon = Globals::get_singleton()->get("application/icon");
				if (appicon!="" && appicon.ends_with(".png")) {
					FileAccess*f = FileAccess::open(appicon,FileAccess::READ);
					if (f) {
						data.resize(f->get_len());
						f->get_buffer(data.ptr(),data.size());
						memdelete(f);
					}
				}
			}
		}

		print_line("ADDING: "+file);
		zipOpenNewFileInZip(apk,
			file.utf8().get_data(),
			NULL,
			NULL,
			0,
			NULL,
			0,
			NULL,
			Z_DEFLATED,
			Z_DEFAULT_COMPRESSION);

		zipWriteInFileInZip(apk,data.ptr(),data.size());
		zipCloseFileInZip(apk);

		ret = unzGoToNextFile(pkg);
	}


	ep.step("Adding Files..",1);



	APKExportData ed;
	ed.ep=&ep;
	ed.apk=apk;

	Error err = export_project_files(save_apk_file,&ed,false);

	zipClose(apk,NULL);
	unzClose(pkg);

	if (err) {
		return err;
	}



	if (_signed) {


		String jarsigner=EditorSettings::get_singleton()->get("android/jarsigner");
		if (!FileAccess::exists(jarsigner)) {
			EditorNode::add_io_error("'jarsigner' could not be found.\nPlease supply a path in the editor settings.\nResulting apk is unsigned.");
			return OK;
		}

		String keystore;
		String password;
		String user;
		if (p_debug) {
			keystore=EditorSettings::get_singleton()->get("android/debug_keystore");
			password=EditorSettings::get_singleton()->get("android/debug_keystore_pass");
			user=EditorSettings::get_singleton()->get("android/debug_keystore_user");

			ep.step("Signing Debug APK..",103);

		} else {
			keystore=release_keystore;
			password=p_password;
			user=release_username;

			ep.step("Signing Release APK..",103);

		}

		if (!FileAccess::exists(keystore)) {
			EditorNode::add_io_error("Could not find keytore, unable to export.");
			return ERR_FILE_CANT_OPEN;
		}

		List<String> args;
		args.push_back("-digestalg");
		args.push_back("SHA1");
		args.push_back("-sigalg");
		args.push_back("MD5withRSA");
		args.push_back("-verbose");
		args.push_back("-keystore");
		args.push_back(keystore);
		args.push_back("-storepass");
		args.push_back(password);
		args.push_back(p_path);
		args.push_back(user);
		int retval;
		int err = OS::get_singleton()->execute(jarsigner,args,true,NULL,NULL,&retval);
		if (retval) {
			EditorNode::add_io_error("'jarsigner' returned with error #"+itos(retval));
			return ERR_CANT_CREATE;
		}

		ep.step("Verifying APK..",104);

		args.clear();
		args.push_back("-verify");
		args.push_back(p_path);
		args.push_back("-verbose");

		err = OS::get_singleton()->execute(jarsigner,args,true,NULL,NULL,&retval);
		if (retval) {
			EditorNode::add_io_error("'jarsigner' verificaiton of APK failed. Make sure to use jarsigner from Java 6.");
			return ERR_CANT_CREATE;
		}

	}
	return OK;

}


bool EditorExportPlatformAndroid::poll_devices() {

	bool dc=devices_changed;
	devices_changed=false;
	return dc;
}

int EditorExportPlatformAndroid::get_device_count() const {

	device_lock->lock();
	int dc=devices.size();
	device_lock->unlock();

	return dc;

}
String EditorExportPlatformAndroid::get_device_name(int p_device) const {

	ERR_FAIL_INDEX_V(p_device,devices.size(),"");
	device_lock->lock();
	String s=devices[p_device].name;
	device_lock->unlock();
	return s;
}
String EditorExportPlatformAndroid::get_device_info(int p_device) const {

	ERR_FAIL_INDEX_V(p_device,devices.size(),"");
	device_lock->lock();
	String s=devices[p_device].description;
	device_lock->unlock();
	return s;
}

void EditorExportPlatformAndroid::_device_poll_thread(void *ud) {

	EditorExportPlatformAndroid *ea=(EditorExportPlatformAndroid *)ud;


	while(!ea->quit_request) {

		String adb=EditorSettings::get_singleton()->get("android/adb");
		if (!FileAccess::exists(adb)) {
			OS::get_singleton()->delay_usec(3000000);
			continue; //adb not configured
		}

		String devices;
		List<String> args;
		args.push_back("devices");
		int ec;
		Error err = OS::get_singleton()->execute(adb,args,true,NULL,&devices,&ec);
		Vector<String> ds = devices.split("\n");
		Vector<String> ldevices;
		for(int i=1;i<ds.size();i++) {

			String d = ds[i];
			int dpos = d.find("device");
			if (dpos==-1)
				continue;
			d=d.substr(0,dpos).strip_edges();
//			print_line("found devuce: "+d);
			ldevices.push_back(d);
		}

		ea->device_lock->lock();

		bool different=false;

		if (devices.size()!=ldevices.size()) {

			different=true;
		} else {

			for(int i=0;i<ea->devices.size();i++) {

				if (ea->devices[i].id!=ldevices[i]) {
					different=true;
					break;
				}
			}
		}

		if (different) {


			Vector<Device> ndevices;

			for(int i=0;i<ldevices.size();i++) {

				Device d;
				d.id=ldevices[i];
				for(int j=0;j<ea->devices.size();j++) {
					if (ea->devices[j].id==ldevices[i]) {
						d.description=ea->devices[j].description;
						d.name=ea->devices[j].name;
					}
				}

				if (d.description=="") {
					//in the oven, request!
					args.clear();
					args.push_back("-s");
					args.push_back(d.id);
					args.push_back("shell");
					args.push_back("cat");
					args.push_back("/system/build.prop");
					int ec;
					String dp;

					Error err = OS::get_singleton()->execute(adb,args,true,NULL,&dp,&ec);
					print_line("RV: "+itos(ec));
					Vector<String> props = dp.split("\n");
					String vendor;
					String device;
					d.description+"Device ID: "+d.id+"\n";
					for(int j=0;j<props.size();j++) {

						String p = props[j];
						if (p.begins_with("ro.product.model=")) {
							device=p.get_slice("=",1).strip_edges();
						} else if (p.begins_with("ro.product.brand=")) {
							vendor=p.get_slice("=",1).strip_edges().capitalize();
						} else if (p.begins_with("ro.build.display.id=")) {
							d.description+="Build: "+p.get_slice("=",1).strip_edges()+"\n";
						} else if (p.begins_with("ro.build.version.release=")) {
							d.description+="Release: "+p.get_slice("=",1).strip_edges()+"\n";
						} else if (p.begins_with("ro.product.cpu.abi=")) {
							d.description+="CPU: "+p.get_slice("=",1).strip_edges()+"\n";
						} else if (p.begins_with("ro.product.manufacturer=")) {
							d.description+="Manufacturer: "+p.get_slice("=",1).strip_edges()+"\n";
						} else if (p.begins_with("ro.board.platform=")) {
							d.description+="Chipset: "+p.get_slice("=",1).strip_edges()+"\n";
						} else if (p.begins_with("ro.opengles.version=")) {
							uint32_t opengl = p.get_slice("=",1).to_int();
							d.description+="OpenGL: "+itos(opengl>>16)+"."+itos((opengl>>8)&0xFF)+"."+itos((opengl)&0xFF)+"\n";
						}
					}

					d.name=vendor+" "+device;
//					print_line("name: "+d.name);
//					print_line("description: "+d.description);

				}

				ndevices.push_back(d);

			}

			ea->devices=ndevices;
			ea->devices_changed=true;
		}

		ea->device_lock->unlock();

		OS::get_singleton()->delay_usec(3000000);
	}

}

Error EditorExportPlatformAndroid::run(int p_device) {

	ERR_FAIL_INDEX_V(p_device,devices.size(),ERR_INVALID_PARAMETER);
	device_lock->lock();

	EditorProgress ep("run","Running on "+devices[p_device].name,3);

	String adb=EditorSettings::get_singleton()->get("android/adb");
	if (adb=="") {

		EditorNode::add_io_error("ADB executable not configured in settings, can't run.");
		device_lock->unlock();
		return ERR_UNCONFIGURED;
	}

	//export_temp
	ep.step("Exporting APK",0);

	String export_to=EditorSettings::get_singleton()->get_settings_path()+"/tmp/tmpexport.apk";
	Error err = export_project(export_to,true);
	if (err) {
		device_lock->unlock();
		return err;
	}

	ep.step("Uninstalling..",1);

	print_line("Uninstalling previous version: "+devices[p_device].name);
	List<String> args;
	args.push_back("-s");
	args.push_back(devices[p_device].id);
	args.push_back("uninstall");
	args.push_back(package);
	int rv;
	err = OS::get_singleton()->execute(adb,args,true,NULL,NULL,&rv);
#if 0
	if (err || rv!=0) {
		EditorNode::add_io_error("Could not install to device.");
		device_lock->unlock();
		return ERR_CANT_CREATE;
	}
#endif
	print_line("Installing into device (please wait..): "+devices[p_device].name);
	ep.step("Installing to Device (please wait..)..",2);

	args.clear();
	args.push_back("-s");
	args.push_back(devices[p_device].id);
	args.push_back("install");
	args.push_back(export_to);
	rv;
	err = OS::get_singleton()->execute(adb,args,true,NULL,NULL,&rv);
	if (err || rv!=0) {
		EditorNode::add_io_error("Could not install to device.");
		device_lock->unlock();
		return ERR_CANT_CREATE;
	}

	ep.step("Running on Device..",3);
	args.clear();
	args.push_back("-s");
	args.push_back(devices[p_device].id);
	args.push_back("shell");
	args.push_back("am");
	args.push_back("start");
	args.push_back("-a");
	args.push_back("android.intent.action.MAIN");
	args.push_back("-n");
	args.push_back(package+"/com.android.godot.Godot");

	err = OS::get_singleton()->execute(adb,args,true,NULL,NULL,&rv);
	if (err || rv!=0) {
		EditorNode::add_io_error("Could not execute ondevice.");
		device_lock->unlock();
		return ERR_CANT_CREATE;
	}
	device_lock->unlock();
	return OK;
}


EditorExportPlatformAndroid::EditorExportPlatformAndroid() {

	version_code=1;
	version_name="1.0";
	package="com.android.noname";
	name="";
	_signed=true;
	device_lock = Mutex::create();
	quit_request=false;
	orientation=0;

	device_thread=Thread::create(_device_poll_thread,this);
	devices_changed=true;

	Image img( _android_logo );
	logo = Ref<ImageTexture>( memnew( ImageTexture ));
	logo->create_from_image(img);
}

bool EditorExportPlatformAndroid::can_export(String *r_error) const {

	bool valid=true;
	String adb=EditorSettings::get_singleton()->get("android/adb");
	String err;

	if (!FileAccess::exists(adb)) {

		valid=false;
		err+="ADB executable not configured in editor settings.\n";
	}

	String js = EditorSettings::get_singleton()->get("android/jarsigner");

	if (!FileAccess::exists(js)) {

		valid=false;
		err+="OpenJDK 6 jarsigner not configured in editor settings.\n";
	}

	String dk = EditorSettings::get_singleton()->get("android/debug_keystore");

	if (!FileAccess::exists(dk)) {

		valid=false;
		err+="Debug Keystore not configured in editor settings.\n";
	}


	String exe_path = EditorSettings::get_singleton()->get_settings_path()+"/templates/";

	if (!FileAccess::exists(exe_path+"android_debug.apk") || !FileAccess::exists(exe_path+"android_release.apk")) {
		valid=false;
		err+="No export templates found.\nDownload and install export templates.\n";
	}

	if (custom_debug_package!="" && !FileAccess::exists(custom_debug_package)) {
		valid=false;
		err+="Custom debug package not found.\n";
	}

	if (custom_release_package!="" && !FileAccess::exists(custom_release_package)) {
		valid=false;
		err+="Custom release package not found.\n";
	}

	if (r_error)
		*r_error=err;

	return valid;
}


EditorExportPlatformAndroid::~EditorExportPlatformAndroid() {

	quit_request=true;
	Thread::wait_to_finish(device_thread);
}


void register_android_exporter() {

	String exe_ext=OS::get_singleton()->get_name()=="Windows"?"exe":"";
	EDITOR_DEF("android/adb","");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING,"android/adb",PROPERTY_HINT_GLOBAL_FILE,exe_ext));
	EDITOR_DEF("android/jarsigner","");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING,"android/jarsigner",PROPERTY_HINT_GLOBAL_FILE,exe_ext));
	EDITOR_DEF("android/debug_keystore","");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING,"android/debug_keystore",PROPERTY_HINT_GLOBAL_FILE,"keystore"));
	EDITOR_DEF("android/debug_keystore_user","androiddebugkey");
	EDITOR_DEF("android/debug_keystore_pass","android");
	//EDITOR_DEF("android/release_keystore","");
	//EDITOR_DEF("android/release_username","");
	//EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING,"android/release_keystore",PROPERTY_HINT_GLOBAL_FILE,"*.keystore"));

	Ref<EditorExportPlatformAndroid> exporter = Ref<EditorExportPlatformAndroid>( memnew(EditorExportPlatformAndroid) );
	EditorImportExport::get_singleton()->add_export_platform(exporter);


}

