/**
 * Copyright (C) 2016 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#include "paletteextended.h"

#include <QSettings>
#include <QString>
#include <QColor>
#include <QMetaEnum>
#include <QFile>
#include <QStyleOption>
#include <QDebug>

#include <private/qcssparser_p.h>

QT_BEGIN_NAMESPACE
QDebug operator<<(QDebug deg, const QCss::Value &value)
{
    deg << value.toString();

    return deg;
}
QDebug operator<<(QDebug deg, const QCss::Selector &selector)
{
    deg << "specificity:" << selector.specificity() << ", pseudoClass:"<< selector.pseudoClass() << ", pseudoElement:" << selector.pseudoElement();

    return deg;
}
QT_END_NAMESPACE

namespace dstyle {

QList<PaletteExtended*> PaletteExtended::styleTypeToPaletteList;
PaletteExtended::PaletteExtended(StyleType type, QObject *parent)
    : QObject(parent)
    , m_brushScheme(new QCss::StyleSheet)
{
    init(type);
}

PaletteExtended *PaletteExtended::instance(StyleType type)
{
    foreach (PaletteExtended *p, styleTypeToPaletteList) {
        if (p->m_type == type)
            return p;
    }

    PaletteExtended *p = new PaletteExtended(type);
    styleTypeToPaletteList << p;

    return p;
}

PaletteExtended::~PaletteExtended()
{
    delete m_brushScheme;
    styleTypeToPaletteList.removeOne(this);
}

static quint64 preprocessClass(qint64 classs)
{
    if (classs & PaletteExtended::PseudoClass_On)
        classs |= PaletteExtended::PseudoClass_Checked;

    if (classs & PaletteExtended::PseudoClass_Checked)
        classs |= PaletteExtended::PseudoClass_On;

    if (classs & PaletteExtended::PseudoClass_Off)
        classs |= PaletteExtended::PseudoClass_Unchecked;

    if (classs & PaletteExtended::PseudoClass_Unchecked)
        classs |= PaletteExtended::PseudoClass_Off;

    if (!(classs & PaletteExtended::PseudoClass_Horizontal))
        classs |= PaletteExtended::PseudoClass_Vertical;

//    if (!(classs & PaletteExtended::PseudoClass_Open))
//        classs |= PaletteExtended::PseudoClass_Closed;

    if (!(classs & PaletteExtended::PseudoClass_Disabled))
        classs |= PaletteExtended::PseudoClass_Enabled;

    return classs;
}

QBrush PaletteExtended::brush(PaletteExtended::BrushName name,  quint64 type, const QBrush &defaultBrush) const
{
    const QPair<BrushName, quint64> &key = qMakePair(name, type);

    if (m_brushCache.contains(key))
        return m_brushCache.value(key);

    QMetaEnum metaEnum = QMetaEnum::fromType<PaletteExtended::BrushName>();
    QString colorName = metaEnum.valueToKey(name);

    const QStringList &path = colorName.split("_");

    foreach (const QCss::StyleRule &rule, m_brushScheme->nameIndex.values(path.first())) {
        bool eligible = false;

        foreach (const QCss::Selector &selector, rule.selectors) {
            if (preprocessClass(selector.pseudoClass()) == type || selector.pseudoClass() == type) {
                eligible = true;
                break;
            }
        }

        if (!eligible)
            continue;

        foreach (const QCss::Declaration &declaration, rule.declarations) {
            if (declaration.d->property == path.last()) {
                QBrush brush = declaration.brushValue();

                if (brush.style() == Qt::NoBrush) {
                    const QString &uri = declaration.uriValue();

                    if (!uri.isEmpty())
                        brush.setTexture(QPixmap(uri));
                }

                m_brushCache[key] = brush;

                return brush;
            }
        }
    }

    m_brushCache[key] = defaultBrush;

    return defaultBrush;
}

static quint64 pseudoClass(QStyle::State state)
{
    quint64 pc = 0;
    if (state & QStyle::State_Enabled) {
        pc |= PaletteExtended::PseudoClass_Enabled;
    }
//    if (state & QStyle::State_Active)
//        pc |= PaletteExtended::PseudoClass_Active;
    if (state & QStyle::State_Window)
        pc |= PaletteExtended::PseudoClass_Window;
    if (state & QStyle::State_On)
        pc |= (PaletteExtended::PseudoClass_On | PaletteExtended::PseudoClass_Checked);
    if (state & QStyle::State_Off)
        pc |= (PaletteExtended::PseudoClass_Off | PaletteExtended::PseudoClass_Unchecked);
    if (state & QStyle::State_NoChange)
        pc |= PaletteExtended::PseudoClass_Indeterminate;
    if (state & QStyle::State_Selected)
        pc |= PaletteExtended::PseudoClass_Selected;
    if (state & QStyle::State_Horizontal)
        pc |= PaletteExtended::PseudoClass_Horizontal;
    else
        pc |= PaletteExtended::PseudoClass_Vertical;
//    if (state & (QStyle::State_Open | QStyle::State_On | QStyle::State_Sunken))
//        pc |= PaletteExtended::PseudoClass_Open;
//    else
//        pc |= PaletteExtended::PseudoClass_Closed;
    if (state & QStyle::State_Children)
        pc |= PaletteExtended::PseudoClass_Children;
    if (state & QStyle::State_Sibling)
        pc |= PaletteExtended::PseudoClass_Sibling;
    if (state & QStyle::State_ReadOnly)
        pc |= PaletteExtended::PseudoClass_ReadOnly;
    if (state & QStyle::State_Item)
        pc |= PaletteExtended::PseudoClass_Item;
#ifdef QT_KEYPAD_NAVIGATION
    if (state & QStyle::State_HasEditFocus)
        pc |= PaletteExtended::PseudoClass_EditFocus;
#endif
    return pc;
}

#define RETURN_BRUSH(Type) {\
    const QBrush &d = brush(name, PseudoClass_##Type, normal);\
    return extraTypes ? brush(name, PseudoClass_##Type | extraTypes, d) : d;\
}

QBrush PaletteExtended::brush(PaletteExtended::BrushName name, const QStyleOption *option, quint64 extraTypes, const QBrush &defaultBrush) const
{
    QBrush normal = brush(name, PseudoClass_Unspecified, defaultBrush);

    if (extraTypes)
        normal = brush(name, extraTypes, normal);

    extraTypes |= pseudoClass(option->state);

    if (const QStyleOptionButton* buttonOption = qstyleoption_cast<const QStyleOptionButton*>(option)) {
        if (buttonOption->features & QStyleOptionButton::Flat)
            extraTypes |= PseudoClass_Flat;

        if (buttonOption->features & QStyleOptionButton::DefaultButton)
            extraTypes |= PseudoClass_Default;
    }

    if (extraTypes)
        normal = brush(name, extraTypes, normal);

    if (!(option->state & QStyle::State_Enabled))
        RETURN_BRUSH(Disabled)
    else if (option->state & QStyle::State_Sunken)
        RETURN_BRUSH(Pressed)
    else if (option->state & QStyle::State_MouseOver)
        RETURN_BRUSH(Hover)
    else if (option->state & QStyle::State_HasFocus)
        RETURN_BRUSH(Focus)

    return normal;
}

void PaletteExtended::polish(QPalette &p)
{
    p.setBrush(QPalette::Base, brush(QPalette_Base));
    p.setBrush(QPalette::Text, brush(QPalette_Text));
    p.setBrush(QPalette::Window, brush(QPalette_Window));
    p.setBrush(QPalette::WindowText, brush(QPalette_WindowText));
    p.setBrush(QPalette::Highlight, brush(QPalette_Highlight));
    p.setBrush(QPalette::HighlightedText, brush(QPalette_HighlightedText));
    p.setBrush(QPalette::Button, brush(QPalette_Button));
    p.setBrush(QPalette::ButtonText, brush(QPalette_ButtonText));

    p.setBrush(QPalette::Disabled, QPalette::Base, brush(QPalette_Base, PseudoClass_Disabled, p.brush(QPalette::Base)));
    p.setBrush(QPalette::Disabled, QPalette::Text, brush(QPalette_Text, PseudoClass_Disabled, p.brush(QPalette::Text)));
    p.setBrush(QPalette::Disabled, QPalette::Window, brush(QPalette_Window, PseudoClass_Disabled, p.brush(QPalette::Window)));
    p.setBrush(QPalette::Disabled, QPalette::WindowText, brush(QPalette_WindowText, PseudoClass_Disabled, p.brush(QPalette::WindowText)));
    p.setBrush(QPalette::Disabled, QPalette::Highlight, brush(QPalette_Highlight, PseudoClass_Disabled, p.brush(QPalette::Highlight)));
    p.setBrush(QPalette::Disabled, QPalette::HighlightedText, brush(QPalette_HighlightedText, PseudoClass_Disabled, p.brush(QPalette::HighlightedText)));
    p.setBrush(QPalette::Disabled, QPalette::Button, brush(QPalette_Button, PseudoClass_Disabled, p.brush(QPalette::Button)));
    p.setBrush(QPalette::Disabled, QPalette::ButtonText, brush(QPalette_ButtonText, PseudoClass_Disabled, p.brush(QPalette::ButtonText)));
}

void PaletteExtended::init(StyleType type)
{
    m_type = type;

    QFile file;

    if (type == StyleType::StyleDark) {
        file.setFileName(":/brushschemes/ddark.css");
    } else if (type == StyleType::StyleLight) {
        file.setFileName(":/brushschemes/dlight.css");
    }

    if (!file.open(QIODevice::ReadOnly))
        return;

    QCss::Parser parser(QString::fromLocal8Bit(file.readAll()));

    parser.parse(m_brushScheme);
}

}
