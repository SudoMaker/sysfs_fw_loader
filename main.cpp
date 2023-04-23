/*
    This file is part of sysfs_fw_loader.

    Copyright (C) 2023 Reimu NotMoe <reimu@sudomaker.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <fstream>
#include <filesystem>
#include <string>
#include <vector>

#include <cstdio>
#include <cstdlib>
#include <climits>

#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/inotify.h>

#include <json-c/json.h>

struct fw_entry {
	std::string name;
	std::string file;
};

std::vector<fw_entry> fw_entries;

__attribute__((noreturn)) inline void fdp(const std::string &err_str) {
	throw std::system_error(errno, std::system_category(), err_str);
}

__attribute__((noreturn)) inline void putain(const std::string &err_str) {
	throw std::logic_error(err_str);
}

void add_fw_entry(const char *name, const char *file) {
	fprintf(stderr, "Registered firmware: [%s] %s\n", name, file);

	fw_entries.emplace_back(fw_entry{name, file});
}

void write_file(const char *path, const void *data, size_t len) {
	int fd = open(path, O_RDWR);

	if (fd < 0)
		fdp(std::string("failed to open file ") + path);

	size_t written = 0;

	while (written < len) {
		ssize_t rc = write(fd, (uint8_t *)data + written, len - written);

		if (rc > 0) {
			written += rc;
		} else if (rc == 0) {
			break;
		} else {
			fdp(std::string("failed to write file ") + path);
		}
	}

	close(fd);
}

void do_load_fw(const std::string &sysfs_path, const std::string &fw_path) {
	std::string loading_path = sysfs_path + "/loading";
	std::string data_path = sysfs_path + "/data";

	write_file(loading_path.c_str(), "1\n", 2);

	if (!fw_path.empty()) {
		int fd_fw = open(fw_path.c_str(), O_RDONLY);

		struct stat sb;
		if (fstat(fd_fw, &sb)) {
			fdp(std::string("failed to stat file ") + loading_path);
		}

		void *fw_data = mmap(nullptr, sb.st_size, PROT_READ, MAP_SHARED, fd_fw, 0);
		if (fw_data == MAP_FAILED) {
			fdp(std::string("failed to mmap file ") + loading_path);
		}

		write_file(data_path.c_str(), fw_data, sb.st_size);

		munmap(fw_data, sb.st_size);
		close(fd_fw);
	}

	write_file(loading_path.c_str(), "0\n", 2);
}

bool try_load_fw(fw_entry &e) {
	for (const auto &it : std::filesystem::directory_iterator("/sys/class/firmware/")) {
		auto fn = it.path().filename().string();

		if (fn.find(e.name) != std::string::npos) {
			fprintf(stderr, "Loading firmware: [%s] -> %s\n", e.name.c_str(), fn.c_str());

			do_load_fw(it.path().string(), e.file);
			return true;
		}
	}

	return false;
}

int main() {
	const char *config_dir = getenv("SYSFS_FW_LOADER_CONFIG_DIR");
	const char *timeout_str = getenv("SYSFS_FW_LOADER_TIMEOUT");

	if (!config_dir) {
		config_dir = "/etc/sysfs_fw_loader/";
	}

	if (!timeout_str) {
		timeout_str = "10";
	}

	for (const auto &it : std::filesystem::directory_iterator(config_dir)) {
		std::ifstream ifs(it.path().string());
		ifs.seekg(0, std::ios::end);
		auto file_size = ifs.tellg();
		ifs.seekg(0, std::ios::beg);

		std::vector<uint8_t> buf(file_size);
		ifs.read((char *)buf.data(), (int)file_size);
		ifs.close();
		buf.push_back(0);

		struct json_object *j_arr;
		j_arr = json_tokener_parse((char *)buf.data());

		if (json_object_is_type(j_arr, json_type_array)) {
			auto array_length = json_object_array_length(j_arr);

			for (unsigned i=0; i<array_length; i++) {
				struct json_object *j_obj = json_object_array_get_idx(j_arr, i);
				struct json_object *name_object, *path_object;

				if (!json_object_object_get_ex(j_obj, "name", &name_object)) {
					putain("json error: no name field");
				}

				if (!json_object_object_get_ex(j_obj, "file", &path_object)) {
					putain("json error: no file field");
				}

				const char *name = json_object_get_string(name_object);
				const char *file = json_object_get_string(path_object);

				add_fw_entry(name, file);
			}
		} else {
			putain("json error: root node is not array");
		}

		json_object_put(j_arr);
	}

	unsigned loaded_count = 0;
	unsigned unchanged_count = 0;
	unsigned timeout_secs = strtol(timeout_str, nullptr, 10);

	while (1) {
		unsigned last_loaded_count = loaded_count;

		for (auto &it: fw_entries) {
			if (try_load_fw(it)) {
				loaded_count++;
				unchanged_count = 0;
			}
		}

		if (loaded_count == fw_entries.size()) {
			break;
		}

		usleep(100 * 1000);

		if (last_loaded_count == loaded_count) {
			unchanged_count++;
		}

		if (unchanged_count > (100 * timeout_secs)) {
			break;
		}
	}

	return 0;
}
