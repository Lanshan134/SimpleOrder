#include "domain/entryparser.h"
#include <stdexcept>

void smartEntryParserTests() {
    auto check = [](bool ok, const char *name) {
        if (!ok)
            throw std::runtime_error(name);
    };
    QDate day(2026, 12, 31);
    auto p = parseEntry("交报告 明天 18:00", day);
    check(p.error.isEmpty() && p.title == "交报告" && p.date == QDate(2027, 1, 1) && p.time == QTime(18, 0),
          "tomorrow across year");
    check(parseEntry("后天下午3点 开会", day).time == QTime(15, 0), "attached natural date and time");
    check(parseEntry("浇花 3天后", day).date == day.addDays(3), "relative days");
    check(parseEntry("开会 下周一", QDate(2026, 10, 25)).date == QDate(2026, 10, 26),
          "next week Sunday boundary");
    check(parseEntry("开会 周一", QDate(2026, 10, 26)).date == QDate(2026, 10, 26), "weekday includes today");
    check(parseEntry("开会 本周一", QDate(2026, 10, 25)).date == QDate(2026, 10, 19),
          "explicit current week");
    check(parseEntry("生日 11月5日", day).date == QDate(2027, 11, 5), "annual date rolls year");
    check(parseEntry("生日 2/29", day).date == QDate(2028, 2, 29), "next valid leap day");
    check(parseEntry("会议 2026-11-05", day).date == QDate(2026, 11, 5), "explicit year kept");
    check(!parseEntry("会议 2026-02-29", day).error.isEmpty(), "invalid leap date rejected");
    check(!parseEntry("会议 4月31日", day).error.isEmpty(), "invalid month date rejected");
    check(parseEntry("学习第3章 项目2026", day).spans.isEmpty(), "ordinary numbers retained");
    check(parseEntry("开会 晚上8点半", day).time == QTime(20, 30), "evening half hour");
    check(parseEntry("开会 18：30", day).time == QTime(18, 30), "fullwidth colon");
    check(parseEntry("开会 凌晨12点", day).time == QTime(0, 0), "midnight");
    check(!parseEntry("开会 25:00", day).error.isEmpty(), "invalid hour rejected");
    check(!parseEntry("开会 下午15点", day).error.isEmpty(), "invalid period hour rejected");
    check(!parseEntry("开会 今天 明天", day).error.isEmpty(), "multiple dates rejected");
    check(!parseEntry("开会 8:00 9:00", day).error.isEmpty(), "multiple times rejected");
    check(!parseEntry("明天 18:00", day).error.isEmpty(), "missing title rejected");
    check(parseEntry("开会 18:00:30", day).spans.isEmpty(), "unsupported seconds not partially extracted");
    check(parseEntry("下午三点开会", day).time == QTime(15, 0) &&
              parseEntry("下午三点开会", day).title == "开会",
          "Chinese hour");
    check(parseEntry("三点半开会", day).time == QTime(3, 30), "Chinese half hour");
    check(parseEntry("开会 十五点二十分", day).time == QTime(15, 20), "Chinese hour and minute units");
    check(parseEntry("三天后开会", day).date == day.addDays(3), "Chinese relative date");
    check(parseEntry("十一月二十五日开会", day).date == QDate(2027, 11, 25), "Chinese month and day");
    check(parseEntry("二〇二八年二月二十九日开会", day).date == QDate(2028, 2, 29),
          "Chinese digit year and leap date");
    check(parseEntry("下午两点零五分开会", day).time == QTime(14, 5), "Chinese liang and leading zero");
    check(!parseEntry("二十五点开会", day).error.isEmpty(), "invalid Chinese hour");
    check(!parseEntry("三点六十分开会", day).error.isEmpty(), "invalid Chinese minute");
    check(!parseEntry("四月三十一日开会", day).error.isEmpty(), "invalid Chinese day");
    check(parseEntry("学习第三点 项目三", day).spans.isEmpty(), "ordinary Chinese numerals retained");
    check(!parseEntry("九九九九九九年三月三日开会", day).error.isEmpty(), "oversized Chinese year rejected");
    const QList<Category> groups{{"work", "工作", "#000000"},      {"room", "工作室", "#000000"},
                                 {"proj", "项目", "#000000"},      {"projA", "项目 A", "#000000"},
                                 {"cpp", "Cpp", "#000000"},        {"mon", "周一例会", "#000000"},
                                 {"today", "今天计划", "#000000"}, {"sharp", "C#", "#000000"},
                                 {"dept", "部门周一例会", "#000000"}};
    auto tag = [&](const char *text, const char *name, const char *id) {
        auto r = parseEntry(text, day, groups);
        return r.error.isEmpty() && r.title == name && r.group == id && r.spans.size() == 1 && !r.date.isValid();
    };
    check(tag("开会 #工作", "开会", "work") && tag("#工作 开会", "开会", "work") &&
              tag("开会 #工作 上午场", "开会 上午场", "work") && tag("开会#工作", "开会", "work") &&
              tag("＃工作 开会", "开会", "work"),
          "tag at the start, middle and end");
    check(tag("开会 #工作室", "开会", "room") && tag("写 #项目 A 报告", "写 报告", "projA") &&
              tag("写 #项目 报告", "写 报告", "proj"),
          "longest category name wins");
    check(tag("学习 #cPP", "学习", "cpp") && tag("学习 #C#", "学习", "sharp"), "case and names holding a hash");
    check(tag("开会 #周一例会", "开会", "mon") && tag("#今天计划 整理", "整理", "today") &&
              tag("开会 #部门周一例会", "开会", "dept"),
          "a date inside a category name is not read");
    p = parseEntry("会议 #工作 明天 18:00", day, groups);
    check(p.title == "会议" && p.group == "work" && p.date == day.addDays(1) && p.time == QTime(18, 0) &&
              p.spans.size() == 3 && p.spans[0].start == 3 && p.spans[0].length == 3,
          "tag beside a date and time, at its source position");
    for (auto text : {"C# 学习", "#1号楼 开会", "issue #12", "issue #12/5 处理", "#3点 会议", "交报告 #明天",
                      "http://a.com/#工作", "开会 #工作组", "开会 ##工作", "F#项目", "#2026-11-05 发布", "＃明天 版本"}) {
        auto r = parseEntry(text, day, groups);
        check(r.error.isEmpty() && r.title == QString(text) && r.group.isEmpty() && r.spans.isEmpty(),
              "unmatched hash text stays in the name");
    }
    check(parseEntry("开会 #工作", day).title == "开会 #工作", "no categories, no tags");
    check(!parseEntry("开会 #工作 #项目", day, groups).error.isEmpty(), "two tags rejected");
    check(!parseEntry("#工作", day, groups).error.isEmpty(), "tag without a name rejected");
    check(parseEntry("买菜", day).title == "买菜" && parseEntry("买菜", day).spans.isEmpty(),
          "plain title unchanged");
}
