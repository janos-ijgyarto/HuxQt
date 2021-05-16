#pragma once

namespace HuxApp
{
	class AppCore;
	class HuxQt;
	struct DisplayData;

	class DisplaySystem
	{
	public:
		class ViewID
		{
		public:
			int get_id() const { return m_id; }
			void invalidate() { m_id = -1; }
			bool is_valid() const { return (m_id != -1); }
		private:
			int m_id = -1;
			friend DisplaySystem;
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

		ViewID register_graphics_view(QGraphicsView* graphics_view);
		void release_graphics_view(const ViewID& view_id, QGraphicsView* graphics_view);

		void update_resources(const QString& resource_path);
		int update_display(const ViewID& view_id, const DisplayData& data);
		void clear_display(const ViewID& view_id);

		const DisplayConfig& get_display_config() const;
		void set_display_config(const DisplayConfig& config);

		const QMap<int, QString>& get_pict_cache() const;

		static int get_page_count(int line_count);
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