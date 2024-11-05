#include <filesystem>
#include <fstream>
#include <string>
#include <list>
#include <iostream>
#include <tuple>
#include <thread>
#include <vector>

#pragma warning(disable:4996)

namespace fs = std::filesystem;
using list_t = std::list<std::wstring>;
using entry_t = std::list<
  std::tuple<
    std::wstring,
    std::chrono::time_point<std::chrono::file_clock>
  >
>;

constexpr auto ignore_file_name = ".backuptoolignore";

void load_ignore_file(list_t& res) {
  std::wifstream ifs(ignore_file_name);
  if ( ! ifs) {
    return;
  }

  std::wstring line;
  while (std::getline(ifs, line)) {
    res.push_back(line);
  }
}

uint32_t get_file_list(const std::wstring& root, std::vector<entry_t>& entry) {
  list_t ignore;
  uint32_t count = 0, pos = 0;
  auto core = std::thread::hardware_concurrency();
  entry.resize(core);

  load_ignore_file(ignore);

  for (const auto& e : fs::recursive_directory_iterator(root)) {
    bool skip = false;
    for (const auto& i : ignore) {
      skip = e.path().native().find(i) != std::wstring::npos;
      if (skip) {
        count++;
        break;
      }
    }
    if ( ! skip && ! e.is_directory()) {
      entry[pos].push_back({e.path(), e.last_write_time()});
      pos++;
      if (core - 1 < pos) {
        pos = 0;
      }
    }
  }
  return count;
}

auto remove_root(const std::wstring& root, const std::wstring& str) {
  return std::move(str.substr(str.find(root) + root.length()));
}

uint32_t copy_files(const entry_t& entry, const std::wstring& root, const std::wstring& dst_root) {
  uint32_t count = 0;

  for (const auto& e : entry) {
    auto& origin_path = std::get<0>(e);
    auto relative_path = remove_root(root, origin_path);
    fs::path dst_path = dst_root + relative_path;

    bool copy = false, over_write = false;
    if (fs::exists(dst_path)) {
      copy = fs::last_write_time(dst_path) < std::get<1>(e);
      if (copy) {
        over_write = true;
      }
    }
    else {
      // remove_filename() is destructive function, so we need another variable
      fs::path tmp = dst_path;
      fs::create_directories(tmp.remove_filename());
      copy = true;
    }

    if (copy) {
      if (over_write) {
        fs::copy(origin_path, dst_path, fs::copy_options::overwrite_existing);
      }
      else {
        fs::copy(origin_path, dst_path);
      }
      count++;
    }
  }
  return count;
}

int main(int argc, char** argv) {
  std::vector<entry_t> entry;
  std::list<std::thread> threads;
  std::wstring src_root = _wgetenv(L"BU_SRC_ROOT_DIR");
  std::wstring dst_root = _wgetenv(L"BU_DST_ROOT_DIR");

  get_file_list(src_root, entry);

  for (const auto& i : entry) {
    threads.emplace_back([&] {
      copy_files(i, src_root, dst_root);
    });
  }

  for (auto& i : threads) {
    i.join();
  }

  return EXIT_SUCCESS;
}
