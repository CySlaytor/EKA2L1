/*
 * Copyright (c) 2018 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <system/software.h>

#include <common/dynamicfile.h>
#include <common/fileutils.h>
#include <common/ini.h>
#include <common/log.h>
#include <common/path.h>
#include <common/pystr.h>

#include <optional>

namespace eka2l1::loader {

    // Enforce Symbian OS v9.3 S60v3 FP2 directly
    epocver determine_rpkg_symbian_version(const std::string &extracted_path) {
        return epocver::epoc93fp2;
    }

    static bool determine_rpkg_product_info_from_platform_txt(const std::string &extracted_path,
        std::string &manufacturer, std::string &firmcode, std::string &model) {
        const std::string version_folder = add_path(extracted_path, "resource\\versions\\");
        std::string product_ini_path = add_path(version_folder, "product.txt");
        common::ini_file product_ini;

        if (product_ini.load(product_ini_path.c_str(), false) != 0) {
            LOG_ERROR(SYSTEM, "Failed to load product.txt.");
            return false;
        }

        manufacturer = product_ini.find("Manufacturer")->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_value();
        firmcode = product_ini.find("Product")->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_value();
        model = product_ini.find("Model")->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_value();

        return true;
    }

    static bool determine_rpkg_product_info_from_various_txts(const std::string &extracted_path,
        std::string &manufacturer, std::string &firmcode, std::string &model) {
        std::string version_folder = add_path(extracted_path, "resource\\versions\\");

        if (!common::exists(version_folder)) {
            version_folder = add_path(extracted_path, "system\\versions\\");
        }

        if (!common::exists(version_folder)) {
            return false;
        }

        common::dynamic_ifile sw_file(add_path(version_folder, "sw.txt"));
        std::string line_buffer;

        if (sw_file.fail()) {
            sw_file = common::dynamic_ifile(add_path(version_folder, "langsw.txt"));

            if (sw_file.fail()) {
                LOG_ERROR(SYSTEM, "Can't load langsw.txt!");
                return false;
            }
        }

        sw_file.getline(line_buffer);

        common::pystr sw_line_py(line_buffer);
        auto sw_infos = sw_line_py.split("\\n");

        firmcode = sw_infos[2].strip_reserverd().std_str();
        manufacturer = "Unknown";

        if (sw_infos.size() >= 4) {
            manufacturer = sw_infos[3].strip_reserverd().std_str();
        }

        common::dynamic_ifile model_file(add_path(version_folder, "model.txt"));
        if (model_file.fail()) {
            LOG_ERROR(SYSTEM, "Can't load model.txt, use backup!");
            model = sw_infos[0].std_str();
        } else {
            model_file.getline(model);
        }

        return true;
    }

    bool determine_rpkg_product_info(const std::string &extracted_path, std::string &manufacturer, std::string &firmcode, std::string &model) {
        if (!determine_rpkg_product_info_from_platform_txt(extracted_path, manufacturer, firmcode, model)) {
            LOG_WARN(SYSTEM, "First method of determining product info failed, start the second one.");

            if (!determine_rpkg_product_info_from_various_txts(extracted_path, manufacturer, firmcode, model)) {
                LOG_ERROR(SYSTEM, "Product info scanning failed.");
                return false;
            }
        }

        return true;
    }
}