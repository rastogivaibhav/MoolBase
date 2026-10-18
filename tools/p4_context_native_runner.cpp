#include "graphene/context_world.hpp"
#include "graphene/predictive_world.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace graphene;

namespace {
constexpr double kPi = 3.14159265358979323846;
struct WeatherRow {
  std::string date;
  double precipitation{0}, mx{0}, mn{0}, wind{0};
};
struct StockRow {
  std::string iso;
  unsigned month{0};
  double price{0};
};

std::vector<std::string> csv_fields(const std::string& line) {
  std::stringstream s(line);
  std::string token;
  std::vector<std::string> parts;
  while (std::getline(s, token, ',')) parts.push_back(token);
  return parts;
}

double number(const std::string& value) {
  size_t n = 0;
  const double d = std::stod(value, &n);
  if (n != value.size() || !std::isfinite(d))
    throw std::runtime_error("nonfinite or malformed input");
  return d;
}

std::chrono::sys_days iso_date(const std::string& date) {
  if (date.size() != 10 || date[4] != '-' || date[7] != '-')
    throw std::runtime_error("unexpected ISO date");
  const auto y = std::chrono::year(std::stoi(date.substr(0,4)));
  const auto m = std::chrono::month(static_cast<unsigned>(std::stoi(date.substr(5,2))));
  const auto d = std::chrono::day(static_cast<unsigned>(std::stoi(date.substr(8,2))));
  const std::chrono::year_month_day ymd(y,m,d);
  if (!ymd.ok()) throw std::runtime_error("invalid ISO date");
  return std::chrono::sys_days(ymd);
}

StockRow parse_stock_date(const std::string& date, const std::string& amount) {
  std::tm tm{};
  std::istringstream s(date);
  s.imbue(std::locale::classic());
  s >> std::get_time(&tm, "%b %d %Y");
  if (s.fail() || s.peek() != std::char_traits<char>::eof())
    throw std::runtime_error("invalid stock date: " + date);
  const auto y = std::chrono::year(1900+tm.tm_year);
  const auto m = std::chrono::month(static_cast<unsigned>(tm.tm_mon+1));
  const auto d = std::chrono::day(static_cast<unsigned>(tm.tm_mday));
  const std::chrono::year_month_day ymd(y,m,d);
  if (!ymd.ok()) throw std::runtime_error("bad Gregorian date");
  std::ostringstream iso;
  iso << std::setfill('0') << std::setw(4) << int(y) << "-"
      << std::setw(2) << unsigned(m) << "-"
      << std::setw(2) << unsigned(d);
  const double p = number(amount);
  if (p <= 0) throw std::runtime_error("invalid stock price");
  return {iso.str(), unsigned(m), p};
}

std::map<std::string,std::vector<WeatherRow>>
weather_rows(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("missing weather dataset");
  std::string line;
  std::getline(in,line);
  if (line != "location,date,precipitation,temp_max,temp_min,wind,weather")
    throw std::runtime_error("weather schema mismatch");
  std::map<std::string,std::vector<WeatherRow>> grouped;
  while (std::getline(in,line)) {
    const auto row=csv_fields(line);
    if(row.size()!=7 || (row[0]!="Seattle" && row[0]!="New York"))
      throw std::runtime_error("weather row schema invalid");
    const double precip=number(row[2]), mx=number(row[3]);
    const double mn=number(row[4]), wind=number(row[5]);
    if (precip<0 || wind<0 || mx<mn) throw std::runtime_error("bad weather number");
    iso_date(row[1]);
    auto& group=grouped[row[0]];
    if(!group.empty() && iso_date(row[1])!=iso_date(group.back().date)+std::chrono::days(1))
      throw std::runtime_error("weather nonchronological or missing day");
    group.push_back({row[1],precip,mx,mn,wind});
  }
  if (grouped.size()!=2) throw std::runtime_error("weather missing city");
  for(const auto& [city, rows]:grouped){
    if(rows.size()!=1461 || rows.front().date!="2012-01-01" ||
       rows.back().date!="2015-12-31") throw std::runtime_error("weather row-count mismatch");
  }
  return grouped;
}

std::map<std::string,std::vector<StockRow>>
stock_rows(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("missing stocks dataset");
  std::string line;
  std::getline(in,line);
  if (line!="symbol,date,price") throw std::runtime_error("finance schema mismatch");
  std::map<std::string,std::vector<StockRow>> stocks;
  while(std::getline(in,line)) {
    const auto row=csv_fields(line);
    if(row.size()!=3 || row[0].empty()) throw std::runtime_error("stock row invalid");
    const auto value=parse_stock_date(row[1],row[2]);
    auto& symbol=stocks[row[0]];
    if(!symbol.empty() && iso_date(value.iso)<=iso_date(symbol.back().iso))
      throw std::runtime_error("duplicate/nonchronological stock date");
    symbol.push_back(value);
  }
  if(stocks.size()<3) throw std::runtime_error("insufficient eligible stock series");
  return stocks;
}

std::vector<double> weather_features(const WeatherRow& today,
                                     const WeatherRow& yesterday) {
  const auto current=iso_date(today.date);
  const int yr=std::stoi(today.date.substr(0,4));
  const auto start=std::chrono::sys_days{
      std::chrono::year(yr)/std::chrono::January/1};
  const double doy=double((current-start).count()+1);
  const double angle=2.0*kPi*doy/365.25;
  return {1.0, yesterday.precipitation>0.0 ? 1.0:0.0,
          yesterday.mx/40.0, yesterday.mn/40.0,
          yesterday.wind/15.0, std::sin(angle), std::cos(angle)};
}

std::vector<double> finance_features(const std::vector<StockRow>& rows,
                                     size_t i) {
  // Exactly four previous prices are available. Nothing from price[i]
  // participates in today's contextual feature snapshot.
  if(i<4) throw std::runtime_error("finance feature warmup absent");
  const auto clamp=[](double x){return std::max(-1.0,std::min(1.0,x));};
  const double r1=clamp(rows[i-1].price/rows[i-2].price-1.0);
  const double r2=clamp(rows[i-2].price/rows[i-3].price-1.0);
  const double r3=clamp(rows[i-3].price/rows[i-4].price-1.0);
  const double mean=(r1+r2+r3)/3.0;
  const double stdev=std::sqrt(((r1-mean)*(r1-mean)+
                               (r2-mean)*(r2-mean)+
                               (r3-mean)*(r3-mean))/3.0);
  const double month=static_cast<double>(rows[i].month);
  const double angle=2.0*kPi*month/12.0;
  return {1.0,r1,r2,mean,stdev,r1>0.0?1.0:0.0,
          std::sin(angle),std::cos(angle)};
}

struct State {
  ModelWorld ledger_baseline;
  ModelWorld ledger_context;
  SinglePredictiveWorld base;
  ContextPredictiveWorld context;
  explicit State(const std::string& world)
      : base(ledger_baseline,world), context(ledger_context,world) {}
};

void run_weather(const std::string& path, std::ofstream& out) {
  auto groups=weather_rows(path);
  uint32_t edge=1;
  for(const auto& [city, rows]:groups) {
    State state(city=="New York"?"NewYork":"Seattle");
    for(size_t i=0;i<rows.size();++i) {
      const auto& today=rows[i];
      WorldForecast b;
      if(!state.base.forecast("rain",today.date,&b))
        throw std::runtime_error("weather native P1 forecast failed");
      ContextWorldForecast c;
      if(i>0 && !state.context.forecast("rain",today.date,rows[i-1].date,
                                       weather_features(today,rows[i-1]),&c))
        throw std::runtime_error("weather native P4 forecast failed");
      // Label is accessed strictly after forecasts have been persisted.
      const bool y=today.precipitation>0.0;
      WorldVerifiedOutcome o{"rain",today.date,y,edge++,
                             "vega_weather_"+city+"_"+today.date,
                             "vega_public_unverified"};
      uint64_t b_id=0,c_id=0;
      if(!state.base.observe(o,&b_id))
        throw std::runtime_error("weather native P1 outcome failed");
      if(i>0){
        if(!state.context.observe(o,&c_id))
          throw std::runtime_error("weather native P4 outcome failed");
        out<<"weather,"<<(city=="New York"?"NewYork":"Seattle")<<","
           <<today.date<<","<<(y?1:0)<<","<<b.probability<<","
           <<c.probability<<"\n";
      }
    }
    if(state.base.metrics("rain").scored_predictions!=1461 ||
       state.context.metrics("rain").verified_observations!=1460)
      throw std::runtime_error("weather final observation count");
    std::cerr<<"weather "<<city<<" native_context_outcomes=1460\n";
  }
}

void run_stocks(const std::string& path, std::ofstream& out) {
  auto groups=stock_rows(path);
  uint32_t edge=5000;
  for(const auto& [name, rows]:groups) {
    State state("ticker_"+name);
    if(rows.size()<54){std::cerr<<"stock "<<name<<" excluded_short\n";continue;}
    size_t counted=0;
    for(size_t i=4;i<rows.size();++i){
      const auto& today=rows[i];
      const bool y_label_access_deferred=true;
      (void)y_label_access_deferred;
      WorldForecast b;
      ContextWorldForecast c;
      if(!state.base.forecast("price_up",today.iso,&b) ||
         !state.context.forecast("price_up",today.iso,rows[i-1].iso,
                                 finance_features(rows,i),&c))
        throw std::runtime_error("finance prior-day forecast failure");
      // Only now read today's price and determine the target label.
      const bool y=today.price>rows[i-1].price;
      WorldVerifiedOutcome o{"price_up",today.iso,y,edge++,
                             "vega_stock_"+name+"_"+today.iso,
                             "vega_public_unverified"};
      uint64_t bid=0,cid=0;
      if(!state.base.observe(o,&bid) || !state.context.observe(o,&cid))
        throw std::runtime_error("finance context evidence update failure");
      out<<"stocks,"<<name<<","<<today.iso<<","<<(y?1:0)
         <<","<<b.probability<<","<<c.probability<<"\n";
      ++counted;
    }
    if(counted<50 ||
       state.base.metrics("price_up").scored_predictions!=counted ||
       state.context.metrics("price_up").verified_observations!=counted)
      throw std::runtime_error("finance stream count mismatch");
    std::cerr<<"finance "<<name<<" observations="<<counted<<"\n";
  }
}

void run(const char* weather,const char* stocks,const char* output) {
  std::ofstream csv(output,std::ios::trunc);
  if(!csv)throw std::runtime_error("cannot write native predictions");
  csv<<"domain,symbol,date,observed,p1,p4\n"<<std::setprecision(17);
  run_weather(weather,csv);
  run_stocks(stocks,csv);
  if(!csv)throw std::runtime_error("native predictions I/O failure");
}

} // namespace

int main(int argc,char**argv) {
  try{
    if(argc!=4)throw std::runtime_error(
        "usage: p4_context_native_runner weather.csv stocks.csv predictions.csv");
    run(argv[1],argv[2],argv[3]);
    std::cout<<"p4_crossdomain_native_execution=PASS\n";
  }catch(const std::exception& ex){
    std::cerr<<"p4_crossdomain_native_error="<<ex.what()<<"\n";
    return 1;
  }
}
