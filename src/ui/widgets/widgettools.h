#pragma once
#include <QtWidgets>
#include <functional>

enum class Glyph { Plus, Left, Right };
QPushButton *glyphButton(Glyph glyph, const QString &label, const QString &name);
QPushButton *button(const QString &text, const QString &name = {});
QLabel *heading(const QString &text, const QString &name = "heading");
QIcon typeDot(const QString &color);
QWidget *quickBar(const QString &placeholder, const std::function<bool(QString)> &add);
void arrangeFieldButtons(QWidget *root);
