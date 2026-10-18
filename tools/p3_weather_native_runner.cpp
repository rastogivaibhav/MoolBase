#include "graphene/predictive_world.hpp"
#include "graphene/regime_world.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;

namespace {
struct InputRow {
  std::string city;
  std::string date;
  double precipitation{0.0};
  double temp_max{0.0};
  double temp_min{0.0};
  double wind{0.0};
};

std::vector<std::string> fields(const std::string& line) {
  std::vector<std::string> out;
  std::stringstream ss(line);
  std::string token;
  while (std::getline(ss, token, ',')) out.push_back(token);
  return out;
}

double numeric(const std::string& value) {
  size_t consumed = 0;
  const double d = std::stod(value, &consumed);
  if (consumed != value.size() || !std::isfinite(d))
    throw std::runtime_error("invalid numeric field");
  return d;
}

std::chrono::sys_days date_day(const std::string& date) {
  if (date.size() != 10 || date[4] != '-' || date[7] != '-')
    throw std::runtime_error("date must be YYYY-MM-DD");
  const int yy = std::stoi(date.substr(0, 4));
  const unsigned mm = static_cast<unsigned>(std::stoi(date.substr(5, 2)));
  const unsigned dd = static_cast<unsigned>(std::stoi(date.substr(8, 2)));
  const std::chrono::year_month_day result{
      std::chrono::year{yy}, std::chrono::month{mm}, std::chrono::day{dd}};
  if (!result.ok()) throw std::runtime_error("invalid calendar date");
  return std::chrono::sys_days{result};
}

std::vector<InputRow> load(const fs::path& csv) {
  std::ifstream in(csv);
  if (!in) throw std::runtime_error("weather source is missing");
  std::string header;
  std::getline(in, header);
  if (header != "location,date,precipitation,temp_max,temp_min,wind,weather")
    throw std::runtime_error("weather source schema changed");
  std::vector<InputRow> result;
  std::map<std::string, std::chrono::sys_days> previous_date;
  std::map<std::string, size_t> per_city;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) throw std::runtime_error("empty source line");
    const auto tokens = fields(line);
    if (tokens.size() != 7 || (tokens[0] != "Seattle" && tokens[0] != "New York"))
      throw std::runtime_error("invalid city or row width");
    const auto date = date_day(tokens[1]);
    const auto prior = previous_date.find(tokens[0]);
    if (prior == previous_date.end()) {
      if (tokens[1] != "2012-01-01") throw std::runtime_error("wrong first date");
    } else if (date != prior->second + std::chrono::days{1}) {
      throw std::runtime_error("missing, repeated or misordered daily observation");
    }
    previous_date.insert_or_assign(tokens[0], date);
    const double precipitation = numeric(tokens[2]);
    const double mx = numeric(tokens[3]);
    const double mn = numeric(tokens[4]);
    const double wind = numeric(tokens[5]);
    if (precipitation < 0 || wind < 0 || mx < mn)
      throw std::runtime_error("invalid meteorological values");
    result.push_back({tokens[0], tokens[1], precipitation, mx, mn, wind});
    ++per_city[tokens[0]];
  }
  if (per_city["Seattle"] != 1461 || per_city["New York"] != 1461 ||
      previous_date["Seattle"] != date_day("2015-12-31") ||
      previous_date["New York"] != date_day("2015-12-31"))
    throw std::runtime_error("missing or extra source observations");
  return result;
}

struct Context {
  ModelWorld base_ledger;
  ModelWorld regime_ledger;
  SinglePredictiveWorld lifetime;
  RegimePredictiveWorld adaptive;
  Context(std::string id)
      : lifetime(base_ledger, id), adaptive(regime_ledger, std::move(id)) {}
};

void run(const fs::path& source, const fs::path& output) {
  const auto rows = load(source);
  std::map<std::string, std::unique_ptr<Context>> worlds;
  worlds["Seattle"] = std::make_unique<Context>("Seattle");
  worlds["New York"] = std::make_unique<Context>("NewYork");

  std::ofstream csv(output, std::ios::trunc);
  if (!csv) throw std::runtime_error("cannot open output file");
  csv << "location,date,observed,p1,p2\n";
  csv << std::setprecision(17);
  uint32_t next_evidence = 1;
  for (const auto& row : rows) {
    Context& world = *worlds.at(row.city);
    const std::string event = "precipitation_positive";
    WorldForecast p1;
    RegimeWorldForecast p2;
    // NB: no access to row.precipitation before BOTH forecasts exist.
    const Status s1 = world.lifetime.forecast(event, row.date, &p1);
    const Status s2 = world.adaptive.forecast(event, row.date, &p2);
    if (!s1 || !s2)
      throw std::runtime_error("native forecast failed at " + row.city + row.date);
    const bool observed = row.precipitation > 0.0;
    const std::string family = "vega_daily_" +
        (row.city == "New York" ? std::string("newyork") : std::string("seattle")) +
        "_" + row.date;
    const WorldVerifiedOutcome o{event, row.date, observed,
                                 next_evidence++, family, "vega_public_unverified"};
    uint64_t outcome_id_1 = 0, outcome_id_2 = 0;
    const Status v1 = world.lifetime.observe(o, &outcome_id_1);
    const Status v2 = world.adaptive.observe(o, &outcome_id_2);
    if (!v1 || !v2)
      throw std::runtime_error("native evidence update failed at " + row.city + row.date);
    csv << row.city << ',' << row.date << ',' << (observed ? 1 : 0) <<
        ',' << p1.probability << ',' << p2.adaptive_probability << '\n';
  }
  if (!csv) throw std::runtime_error("failed writing native output");
  for (const auto& kv : worlds) {
    auto& world = *kv.second;
    const auto p1 = world.lifetime.metrics("precipitation_positive");
    const auto p2 = world.adaptive.report("precipitation_positive");
    if (p1.scored_predictions != 1461 || p2.verified_observations != 1461)
      throw std::runtime_error("native verification count mismatch");
    std::cerr << kv.first << ": verified=" << p2.verified_observations
              << ", provisional_regimes=" << p2.regime_changes << '\n';
  }
  std::cout << "p3_native_complete=PASS rows=" << rows.size() << '\n';
}
}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc != 3)
      throw std::runtime_error("usage: native_runner weather.csv predictions.csv");
    run(argv[1], argv[2]);
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "p3_native_error: " << e.what() << '\n';
    return 1;
  }
}
