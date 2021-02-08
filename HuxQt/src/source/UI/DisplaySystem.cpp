#include "stdafx.h"
#include "UI/DisplaySystem.h"
#include "UI/DisplayData.h"

#include "UI/HuxQt.h"

#include "Utils/Utilities.h"

namespace HuxApp
{
	namespace
	{
		// Values taken directly from Aleph One source code :(
		constexpr qreal TERMINAL_WIDTH = 640.0;
		constexpr qreal TERMINAL_HEIGHT = 320.0;
		constexpr qreal TERMINAL_BORDER_HEIGHT = 18;

		// Text rendering constants
		// NOTE: these values are just about good enough to create WYSIWYG between the tool and Aleph One (some of the word wrapping won't be 100% there)
		// The preview config window can be used for further tweaking
		constexpr qreal DEFAULT_LINE_SPACING = -4.0;
		constexpr qreal DEFAULT_WORD_SPACING = -1.0;
		constexpr qreal DEFAULT_LETTER_SPACING = -1.0;
		constexpr qreal DEFAULT_HORIZONTAL_MARGIN = 72;
		constexpr qreal DEFAULT_VERTICAL_MARGIN = 27;

		constexpr const char* MISSING_RESOURCE_IMAGE = ":/HuxQt/missing.png";

		enum class DisplayColors
		{
			BACKGROUND,
			BORDER,
			TEXT,
			COLOR_COUNT
		};

		const QColor DISPLAY_COLOR_ARRAY[Utils::to_integral(DisplayColors::COLOR_COUNT)] = {
			{ 0, 0, 0, 255 },
			{ 100, 0, 0, 255 },
			{ 0, 255, 0, 255 }
		};

		QColor get_display_color(DisplayColors color) { return DISPLAY_COLOR_ARRAY[Utils::to_integral(color)]; }
	}

	struct DisplaySystem::ViewData
	{
		DisplayData m_display_data;
		QGraphicsScene m_scene;

		QGraphicsPixmapItem* m_image_item = nullptr;
		QGraphicsTextItem* m_text_item = nullptr;
	};

	struct DisplaySystem::Internal
	{
		Internal()
			: m_font("Courier")
		{
			// Set the configurations for each scene
			m_font.setPointSize(1);

			for (int scene_data_index = 0; scene_data_index < Utils::to_integral(DisplaySystem::View::VIEW_COUNT); ++scene_data_index)
			{
				ViewData& current_view_data = m_view_data[scene_data_index];

				current_view_data.m_scene.setSceneRect(QRectF(0, 0, TERMINAL_WIDTH, TERMINAL_HEIGHT));
				current_view_data.m_scene.setBackgroundBrush(QBrush(get_display_color(DisplayColors::BACKGROUND)));

				// Add top and bottom rectangles
				QGraphicsRectItem* border_rect = current_view_data.m_scene.addRect(0, 0, TERMINAL_WIDTH, TERMINAL_BORDER_HEIGHT);
				border_rect->setBrush(QBrush(get_display_color(DisplayColors::BORDER)));
				border_rect->setZValue(3);

				border_rect = current_view_data.m_scene.addRect(0, TERMINAL_HEIGHT - TERMINAL_BORDER_HEIGHT, TERMINAL_WIDTH, TERMINAL_BORDER_HEIGHT);
				border_rect->setBrush(QBrush(get_display_color(DisplayColors::BORDER)));
				border_rect->setZValue(3);

				// Set the text item
				current_view_data.m_text_item = current_view_data.m_scene.addText("");
				current_view_data.m_text_item->setDefaultTextColor(get_display_color(DisplayColors::TEXT));
				current_view_data.m_text_item->setZValue(2);

				// Set the image item
				current_view_data.m_image_item = current_view_data.m_scene.addPixmap(QPixmap());
				current_view_data.m_image_item->setZValue(1);
			}

			// Set the default config
			m_display_config.m_lineSpacing = DEFAULT_LINE_SPACING;
			m_display_config.m_wordSpacing = DEFAULT_WORD_SPACING;
			m_display_config.m_letterSpacing = DEFAULT_LETTER_SPACING;
			m_display_config.m_horizontalMargin = DEFAULT_HORIZONTAL_MARGIN;
			m_display_config.m_verticalMargin = DEFAULT_VERTICAL_MARGIN;

			update_config();
		}

		QPixmap get_pict(int pict_id) const
		{
			if (m_pict_path_cache.contains(pict_id))
			{
				const QString& pict_path = m_pict_path_cache[pict_id];
				return QPixmap(pict_path);
			}

			return QPixmap(MISSING_RESOURCE_IMAGE);
		}

		void apply_display_config(const DisplayConfig& config)
		{
			m_display_config = config;
			update_config();
		}

		void update_config()
		{
			m_font.setWordSpacing(m_display_config.m_wordSpacing);
			m_font.setLetterSpacing(QFont::AbsoluteSpacing, m_display_config.m_letterSpacing);

			for (int scene_data_index = 0; scene_data_index < Utils::to_integral(DisplaySystem::View::VIEW_COUNT); ++scene_data_index)
			{
				m_view_data[scene_data_index].m_text_item->setFont(m_font);
			}
		}

		// Cache for PICT resources used in terminals
		QMap<int, QString> m_pict_path_cache;
		
		// Store data per-view
		ViewData m_view_data[Utils::to_integral(DisplaySystem::View::VIEW_COUNT)];

		// Cache for display configuration data (font, spacing, etc.)
		QFont m_font;
		DisplayConfig m_display_config;
	};

	DisplaySystem::~DisplaySystem() = default;

	void DisplaySystem::set_graphics_view(View view, QGraphicsView* graphics_view)
	{
		ViewData& selected_view = m_internal->m_view_data[Utils::to_integral(view)];
		graphics_view->setScene(&selected_view.m_scene);
		graphics_view->show();
	}

	void DisplaySystem::update_resources(const QString& resource_path)
	{
		// Clear the pict cache
		m_internal->m_pict_path_cache.clear();

		for (int scene_data_index = 0; scene_data_index < Utils::to_integral(DisplaySystem::View::VIEW_COUNT); ++scene_data_index)
		{
			ViewData& current_view_data = m_internal->m_view_data[scene_data_index];

			current_view_data.m_display_data = DisplayData();
			current_view_data.m_text_item->setPlainText("");
			current_view_data.m_image_item->setVisible(false);
		}

		// Go through the "PICT" directory
		const QDir resource_dir(resource_path + "/PICT");
		assert(resource_dir.exists());

		const QFileInfoList pict_file_list = resource_dir.entryInfoList(QDir::Files);
		for (const QFileInfo& current_pict_file : pict_file_list)
		{
			if (!current_pict_file.isFile())
			{
				continue;
			}

			// Add the pict path to the cache
			const QString pict_path = current_pict_file.filePath();
			const int pict_id = current_pict_file.baseName().toInt();

			m_internal->m_pict_path_cache.insert(pict_id, pict_path);
		}
	}

	void DisplaySystem::update_display(View view, const DisplayData& data)
	{
		ViewData& selected_view = m_internal->m_view_data[Utils::to_integral(view)];
		if (data == selected_view.m_display_data)
		{
			return;
		}

		// Update the display contents
		update_image(selected_view, data);
		update_text(selected_view, data);

		// Update the screen type
		selected_view.m_display_data.m_screen_type = data.m_screen_type;
	}

	const DisplaySystem::DisplayConfig& DisplaySystem::get_display_config() const { return m_internal->m_display_config; }

	void DisplaySystem::set_display_config(const DisplayConfig& config) { m_internal->apply_display_config(config); }

	const QMap<int, QString>& HuxApp::DisplaySystem::get_pict_cache() const { return m_internal->m_pict_path_cache; }

	DisplaySystem::DisplaySystem(AppCore& core)
		: m_internal(std::make_unique<Internal>())
	{
	}

	void DisplaySystem::update_image(ViewData& view, const DisplayData& data)
	{
		switch (data.m_screen_type)
		{
		case Terminal::ScreenType::INFORMATION:
		case Terminal::ScreenType::CHECKPOINT:
			// Hide image, text only!
			view.m_image_item->setVisible(false);
			return;
		default:
			view.m_image_item->setVisible(true);
			break;
		}

		if (data.m_resource_id != view.m_display_data.m_resource_id)
		{
			// Load the new resource (code is only reachable when the screen has an image)
			view.m_image_item->setPixmap(m_internal->get_pict(data.m_resource_id));
			view.m_display_data.m_resource_id = data.m_resource_id;
		}

		// Reposition according to the type
		const QRectF image_rect = view.m_image_item->boundingRect();

		switch (data.m_screen_type)
		{
		case Terminal::ScreenType::LOGON:
		case Terminal::ScreenType::LOGOFF:
			// Move to center
			view.m_image_item->setPos(((TERMINAL_WIDTH - image_rect.width()) * 0.5) , (TERMINAL_HEIGHT - image_rect.height()) * 0.5);
			break;
		case Terminal::ScreenType::PICT:
			// Move to side
			view.m_image_item->setPos(((TERMINAL_WIDTH * 0.5) - image_rect.width()) * 0.5, (TERMINAL_HEIGHT - image_rect.height()) * 0.5);
			break;
		}
	}

	void DisplaySystem::update_text(ViewData& view, const DisplayData& data)
	{
		// Use HTML to handle formatting
		view.m_display_data.m_text = data.m_text;
		view.m_text_item->setHtml(view.m_display_data.m_text);

		// Reposition according to type
		switch (data.m_screen_type)
		{
		case Terminal::ScreenType::LOGON:
		case Terminal::ScreenType::LOGOFF:
		{
			// Set center, below image
			view.m_text_item->adjustSize();
			const QRectF text_rect = view.m_text_item->boundingRect();
			const QRectF image_rect = view.m_image_item->boundingRect();
			view.m_text_item->setPos((TERMINAL_WIDTH - text_rect.width()) * 0.5, (TERMINAL_HEIGHT + image_rect.height()) * 0.5);
			view.m_text_item->setTextWidth(TERMINAL_WIDTH - (2 * m_internal->m_display_config.m_horizontalMargin));
		}
			break;
		case Terminal::ScreenType::INFORMATION:
			// NOTE: values taken from AO source code, will need to make it adaptable
			view.m_text_item->setPos(m_internal->m_display_config.m_horizontalMargin, m_internal->m_display_config.m_verticalMargin);
			view.m_text_item->setTextWidth(TERMINAL_WIDTH - (2 * m_internal->m_display_config.m_horizontalMargin));
			break;
		case Terminal::ScreenType::PICT:
		case Terminal::ScreenType::CHECKPOINT:
			// Move to the side
			view.m_text_item->setPos(TERMINAL_WIDTH * 0.5, TERMINAL_BORDER_HEIGHT);
			view.m_text_item->setTextWidth(TERMINAL_WIDTH * 0.5);
			break;
		}

		// Have to do this to make sure the line spacing is correct (might be redundant?)
		update_line_spacing(view);
	}

	void DisplaySystem::update_line_spacing(ViewData& view)
	{
		QTextBlockFormat format;
		format.setLineHeight(m_internal->m_display_config.m_lineSpacing, QTextBlockFormat::LineDistanceHeight);

		QTextCursor doc_cursor(view.m_text_item->document());
		doc_cursor.select(QTextCursor::Document);
		doc_cursor.mergeBlockFormat(format);
	}
}