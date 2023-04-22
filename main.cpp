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

#include <cstdio>
#include <cstdlib>

#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>

#include "json.hpp"

using json = nlohmann::json;



struct fw_entry {
	std::string name;
	std::string file;
	bool done = false;
};

std::vector<fw_entry> fw_entries;

void add_fw_entry(json &j) {
	std::string s_name = j["name"];
	std::string s_file = j["file"];

	fprintf(stderr, "Registered firmware: [%s] %s\n", s_name.c_str(), s_file.c_str());

	fw_entries.emplace_back(fw_entry{std::move(s_name), std::move(s_file)});
}

void write_file(const char *path, const void *data, size_t len) {
	int fd = open(path, O_RDWR);

	if (fd < 0)
		throw std::system_error(errno, std::system_category(), std::string("failed to open file ") + path);

	size_t written = 0;

	while (written < len) {
		ssize_t rc = write(fd, (uint8_t *)data + written, len - written);

		if (rc > 0) {
			written += rc;
		} else if (rc == 0) {
			break;
		} else {
			throw std::system_error(errno, std::system_category(), std::string("failed to write file ") + path);
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
			throw std::system_error(errno, std::system_category(), std::string("failed to stat file ") + loading_path);
		}

		void *fw_data = mmap(nullptr, sb.st_size, PROT_READ, MAP_SHARED, fd_fw, 0);
		if (fw_data == MAP_FAILED) {
			throw std::system_error(errno, std::system_category(), std::string("failed to mmap file ") + loading_path);
		}

		write_file(data_path.c_str(), fw_data, sb.st_size);

		munmap(fw_data, sb.st_size);
		close(fd_fw);
	}

	write_file(loading_path.c_str(), "0\n", 2);
}

void try_load_fw(fw_entry &e) {
	for (const auto &it : std::filesystem::directory_iterator("/sys/class/firmware/")) {
		auto fn = it.path().filename().string();

		if (fn.find(e.name) != std::string::npos) {
			fprintf(stderr, "Loading firmware: [%s] -> %s\n", e.name.c_str(), fn.c_str());

			do_load_fw(it.path().string(), e.file);
			e.done = true;
			break;
		}
	}
}

int main() {
	const char *config_dir = getenv("SYSFS_FW_LOADER_CONFIG_DIR");

	if (!config_dir) {
		config_dir = "/etc/sysfs_fw_loader/";
	}

	for (const auto &it : std::filesystem::directory_iterator(config_dir)) {
		std::ifstream ifs(it.path().string());
		json j;

		ifs >> j;

		if (j.is_object()) {
			add_fw_entry(j);
		} else if (j.is_array()) {
			for (auto &iu: j) {
				if (iu.is_object()) {
					add_fw_entry(iu);
				}
			}
		}
	}

	while (1) {
		size_t remaining = 0;

		for (auto &it: fw_entries) {
			try_load_fw(it);
			if (!it.done)
				remaining++;
		}

		if (remaining == 0)
			break;

		usleep(10 * 1000);
	}

	return 0;
}
