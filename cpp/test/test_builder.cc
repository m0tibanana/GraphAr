/** Copyright 2022 Alibaba Group Holding Limited.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include <time.h>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "arrow/adapters/orc/adapter.h"
#include "arrow/api.h"
#include "arrow/csv/api.h"
#include "arrow/filesystem/api.h"
#include "arrow/io/api.h"
#include "arrow/stl.h"
#include "arrow/util/uri.h"
#include "parquet/arrow/writer.h"

#include "./util.h"
#include "gar/graph_info.h"
#include "gar/writer/arrow_chunk_writer.h"
#include "gar/writer/edges_builder.h"
#include "gar/writer/vertices_builder.h"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

TEST_CASE("test_vertices_builder") {
  std::string root;
  REQUIRE(GetTestResourceRoot(&root).ok());

  std::string vertex_meta_file =
      root + "/ldbc_sample/parquet/" + "person.vertex.yml";
  auto vertex_meta = GAR_NAMESPACE::Yaml::LoadFile(vertex_meta_file).value();
  auto vertex_info = GAR_NAMESPACE::VertexInfo::Load(vertex_meta).value();
  GAR_NAMESPACE::IdType start_index = 0;
  GAR_NAMESPACE::builder::VerticesBuilder builder(vertex_info, "/tmp/",
                                                  start_index);

  std::ifstream fp(root + "/ldbc_sample/person_0_0.csv");
  std::string line;
  getline(fp, line);
  int m = 4;
  std::vector<std::string> names;
  std::istringstream readstr(line);
  for (int i = 0; i < m; i++) {
    std::string name;
    getline(readstr, name, '|');
    names.push_back(name);
  }
  int index = 0;
  while (getline(fp, line)) {
    std::string val;
    std::istringstream readstr(line);
    GAR_NAMESPACE::builder::Vertex v;
    for (int i = 0; i < m; i++) {
      getline(readstr, val, '|');
      if (i == 0) {
        int64_t x = 0;
        for (size_t j = 0; j < val.length(); j++)
          x = x * 10 + val[j] - '0';
        v.AddProperty(names[i], x);
      } else {
        v.AddProperty(names[i], val);
      }
    }
    index++;
    REQUIRE(builder.AddVertex(v).ok());
  }
  REQUIRE(builder.Dump().ok());

  auto fs = arrow::fs::FileSystemFromUriOrPath(root).ValueOrDie();
  auto input =
      fs->OpenInputStream("/tmp/vertex/person/vertex_count").ValueOrDie();
  auto num = input->Read(sizeof(GAR_NAMESPACE::IdType)).ValueOrDie();
  GAR_NAMESPACE::IdType* ptr = (GAR_NAMESPACE::IdType*) num->data();
  REQUIRE((*ptr) == start_index + builder.GetNum());
}

TEST_CASE("test_edges_builder") {
  std::string root;
  REQUIRE(GetTestResourceRoot(&root).ok());

  std::string edge_meta_file =
      root + "/ldbc_sample/parquet/" + "person_knows_person.edge.yml";
  auto edge_meta = GAR_NAMESPACE::Yaml::LoadFile(edge_meta_file).value();
  auto edge_info = GAR_NAMESPACE::EdgeInfo::Load(edge_meta).value();
  GAR_NAMESPACE::builder::EdgesBuilder builder(
      edge_info, "/tmp/", GraphArchive::AdjListType::ordered_by_dest, 903);

  std::ifstream fp(root + "/ldbc_sample/person_knows_person_0_0.csv");
  std::string line;
  getline(fp, line);
  std::vector<std::string> names;
  std::istringstream readstr(line);
  std::map<std::string, int64_t> mapping;
  int64_t cnt = 0;
  while (getline(fp, line)) {
    std::string val;
    std::istringstream readstr(line);
    int64_t s, d;
    for (int i = 0; i < 3; i++) {
      getline(readstr, val, '|');
      if (i == 0) {
        if (mapping.find(val) == mapping.end())
          mapping[val] = cnt++;
        s = mapping[val];
      } else if (i == 1) {
        if (mapping.find(val) == mapping.end())
          mapping[val] = cnt++;
        d = mapping[val];
      } else {
        GAR_NAMESPACE::builder::Edge e(s, d);
        e.AddProperty("creationDate", val);
        REQUIRE(builder.AddEdge(e).ok());
      }
    }
  }
  std::cout << "Test edge builder" << std::endl;
  REQUIRE(builder.Dump().ok());
}
