#pragma once
#include <QColor>
namespace HuxApp
{
	namespace Utils
	{
        inline QColor get_contrast_color(const QColor& color)
        {
            // Code borrowed from: https://stackoverflow.com/a/1855903
            // Counting the perceptive luminance - human eye favors green color...      
            const double luminance = (0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue()) / 255;

            if (luminance > 0.5)
            {
                return Qt::black; // bright colors - black font
            }
            else
            {
                return Qt::white; // dark colors - white font
            }
        }
	}
}