#pragma once
#include <QtWidgets>
#include <functional>

enum class Glyph { Plus, Left, Right, Calendar, List, Manager, Settings };
// Swaps a glyph button's icon together with its tooltip and accessible name.
void setGlyph(QPushButton *button, Glyph glyph, const QString &label);
QPushButton *glyphButton(Glyph glyph, const QString &label, const QString &name);
QPushButton *button(const QString &text, const QString &name = {});
QLabel *heading(const QString &text, const QString &name = "heading");
QIcon typeDot(const QString &color);
QWidget *quickBar(const QString &placeholder, const std::function<bool(QString)> &add);
void arrangeFieldButtons(QWidget *root);
