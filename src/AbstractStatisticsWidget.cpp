/***********************************************************************
 *
 * Copyright (C) 2016 wereturtle
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ***********************************************************************/

#include <QHBoxLayout>

#include "AbstractStatisticsWidget.h"

AbstractStatisticsWidget::AbstractStatisticsWidget(QWidget* parent)
    : QListWidget(parent),
      LESS_THAN_ONE_MINUTE_STR(tr("&lt; 1m")),
      LESS_THAN_ONE_STR(tr("&lt; 1")),
      PAGE_STATISTIC_INFO_TOOLTIP_STR(tr("250 words per page"))
{

}

AbstractStatisticsWidget::~AbstractStatisticsWidget()
{

}

void AbstractStatisticsWidget::setIntegerValueForLabel(QLabel* label, int value)
{
    label->setText(QString("<b>%L1</b>").arg(value));
}

void AbstractStatisticsWidget::setStringValueForLabel(QLabel* label, const QString& value)
{
    label->setText(QString("<b>") + value + "</b>");
}

void AbstractStatisticsWidget::setPercentageValueForLabel(QLabel* label, int percentage)
{
    label->setText(QString("<b>%L1%</b>").arg(percentage));
}

void AbstractStatisticsWidget::setTimeValueForLabel(QLabel* label, int minutes)
{
    QString timeText;

    int hours = minutes / 60;

    if (minutes < 1)
    {
        timeText = QString("<b>") + LESS_THAN_ONE_MINUTE_STR + "</b>";
    }
    else if (hours > 0)
    {
        timeText = QString("<b>") + tr("%1h %2m").arg(hours).arg(minutes % 60) + "</b>";
    }
    else
    {
        timeText = QString("<b>") + tr("%1m").arg(minutes) + "</b>";
    }

    label->setText(timeText);
}

void AbstractStatisticsWidget::setPageValueForLabel(QLabel* label, int pages)
{
    QString pagesText;

    if (pages < 1)
    {
        pagesText = QString("<b>") + LESS_THAN_ONE_STR + "</b>";
    }
    else
    {
        pagesText = QString("<b>%L1</b>").arg(pages);
    }

    label->setText(pagesText);
}

QLabel* AbstractStatisticsWidget::addStatisticLabel
(
    const QString& description,
    const QString& initialValue,
    const QString& toolTip
)
{
    QHBoxLayout* layout = new QHBoxLayout();
    layout->setContentsMargins(2, 2, 2, 2);

    QLabel* descriptionLabel = new QLabel(description);
    descriptionLabel->setAlignment(Qt::AlignRight);

    QLabel* valueLabel = new QLabel(QString("<b>%1</b>").arg(initialValue));

    valueLabel->setTextFormat(Qt::RichText);
    valueLabel->setAlignment(Qt::AlignLeft);

    layout->addWidget(descriptionLabel);
    layout->addWidget(valueLabel);
    layout->setSizeConstraint(QLayout::SetMinimumSize);

    QWidget* containerWidget = new QWidget();
    containerWidget->setLayout(layout);

    QListWidgetItem* item = new QListWidgetItem();
    item->setSizeHint(containerWidget->sizeHint());

    this->addItem(item);
    this->setItemWidget(item, containerWidget);

    if (!toolTip.isNull())
    {
        descriptionLabel->setToolTip(toolTip);
        valueLabel->setToolTip(toolTip);
    }

    return valueLabel;
}
