#pragma once

namespace HuxApp
{
	class AppCore;
	class HuxQt;
	struct DisplayData;

	class DisplaySystem
	{
	public:
		enum class View
		{
			MAIN_WINDOW,
			SCREEN_EDITOR,
			VIEW_COUNT
		};

		struct DisplayConfig
		{
			qreal m_lineSpacing = 0;
			qreal m_wordSpacing = 0;
			qreal m_letterSpacing = 0;
			qreal m_horizontalMargin = 0;
			qreal m_verticalMargin = 0;
		};

		~DisplaySystem();

		void set_graphics_view(View view, QGraphicsView* graphics_view);
		void update_resources(const QString& resource_path);
		void update_display(View view, const DisplayData& data);
		void clear_display(View view);

		const DisplayConfig& get_display_config() const;
		void set_display_config(const DisplayConfig& config);

		const QMap<int, QString>& get_pict_cache() const;
	private:
		struct ViewData;

		DisplaySystem(AppCore& core);
		void update_image(ViewData& view, const DisplayData& data);
		void update_text(ViewData& view, const DisplayData& data);
		void update_line_spacing(ViewData& view);

		struct Internal;
		std::unique_ptr<Internal> m_internal;

		friend AppCore;
	};
}