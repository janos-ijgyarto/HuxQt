#include <HuxQt/UI/DisplaySystem.h>
#include <HuxQt/UI/DisplayData.h>

#include <HuxQt/UI/HuxQt.h>

#include <HuxQt/Utils/Utilities.h>

#include <QTextDocument>
#include <QTextBlock>
#include <QFile>
#include <QDir>
#include <QGraphicsItem>

namespace HuxApp
{
	namespace
	{
		// Values taken directly from Aleph One source code :(
		constexpr qreal TERMINAL_WIDTH = 640.0;
		constexpr qreal TERMINAL_HEIGHT = 320.0;
		constexpr qreal TERMINAL_BORDER = 18;

		// Value taken from the game
		constexpr int SCREEN_MAX_LINES = 22;

		// Text rendering constants
		// NOTE: these values are just about good enough to create WYSIWYG between the tool and Aleph One (some of the word wrapping won't be 100% there)
		// The preview config window can be used for further tweaking
		constexpr qreal DEFAULT_LINE_SPACING = -4.00;
		constexpr qreal DEFAULT_WORD_SPACING = -0.60;
		constexpr qreal DEFAULT_LETTER_SPACING = -1.25;
		constexpr qreal DEFAULT_HORIZONTAL_MARGIN = 72;
		constexpr qreal DEFAULT_VERTICAL_MARGIN = 27;

		constexpr qreal LINE_NUMBER_LEFT_OFFSET = 1.0;
		constexpr qreal LINE_NUMBER_RIGHT_OFFSET = TERMINAL_BORDER - 2.0;

		constexpr const char* MISSING_RESOURCE_IMAGE = ":/HuxQt/missing.png";
		constexpr const char* STATIC_SCREEN_IMAGE = ":/HuxQt/static.png";

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

		int get_text_document_line_count(const QTextDocument* text_document)
		{
			text_document->documentLayout();

			int line_count = 0;
			for (int current_block = 0; current_block < text_document->blockCount(); ++current_block)
			{
				line_count += text_document->findBlockByNumber(current_block).layout()->lineCount();
			}

			return line_count;
		}

		int get_screen_character_limit(Terminal::ScreenType screen_type)
		{
			switch (screen_type)
			{
				case Terminal::ScreenType::INFORMATION:
					return 70;
				case Terminal::ScreenType::PICT:
				case Terminal::ScreenType::CHECKPOINT:
					return 43;
			}

			return -1;
		}
	}

	struct DisplaySystem::ViewData
	{
		DisplayData m_display_data;
		QGraphicsScene m_scene;

		QGraphicsPixmapItem* m_image_item = nullptr;
		QGraphicsTextItem* m_text_item = nullptr;
		QGraphicsTextItem* m_line_numbers = nullptr;
	};

	struct DisplaySystem::Internal
	{
		Internal()
			: m_font("Courier")
		{
			// Set the configurations for each scene
			m_font.setPointSizeF(10.f);
			m_font.setStyleHint(QFont::Monospace);
			m_font.setKerning(false);

			// Set the default config
			m_display_config.m_line_spacing = DEFAULT_LINE_SPACING;
			m_display_config.m_word_spacing = DEFAULT_WORD_SPACING;
			m_display_config.m_letter_spacing = DEFAULT_LETTER_SPACING;
			m_display_config.m_horizontal_margin = DEFAULT_HORIZONTAL_MARGIN;
			m_display_config.m_vertical_margin = DEFAULT_VERTICAL_MARGIN;

			update_config();
		}

		void init_view(ViewData& view_data)
		{
			view_data.m_scene.setSceneRect(QRectF(0, 0, TERMINAL_WIDTH, TERMINAL_HEIGHT));
			view_data.m_scene.setBackgroundBrush(QBrush(get_display_color(DisplayColors::BACKGROUND)));

			// Add top and bottom rectangles
			QGraphicsRectItem* border_rect = view_data.m_scene.addRect(0, 0, TERMINAL_WIDTH, TERMINAL_BORDER);
			border_rect->setBrush(QBrush(get_display_color(DisplayColors::BORDER)));
			border_rect->setZValue(3);

			border_rect = view_data.m_scene.addRect(0, TERMINAL_HEIGHT - TERMINAL_BORDER, TERMINAL_WIDTH, TERMINAL_BORDER);
			border_rect->setBrush(QBrush(get_display_color(DisplayColors::BORDER)));
			border_rect->setZValue(3);

			// Set the text item
			view_data.m_text_item = view_data.m_scene.addText("");
			view_data.m_text_item->setDefaultTextColor(get_display_color(DisplayColors::TEXT));
			view_data.m_text_item->setZValue(2);

			// Set the text font
			view_data.m_text_item->setFont(m_font);

			// Set the image item
			view_data.m_image_item = view_data.m_scene.addPixmap(QPixmap());
			view_data.m_image_item->setZValue(1);

			// Set the line number item
			{
				QStringList line_number_list;
				for (int current_line_number = 1; current_line_number <= SCREEN_MAX_LINES; ++current_line_number)
				{
					line_number_list << QString::number(current_line_number);
				}
				line_number_list << "=";

				view_data.m_line_numbers = view_data.m_scene.addText(line_number_list.join("\n"));

				view_data.m_line_numbers->setPos(LINE_NUMBER_LEFT_OFFSET, DEFAULT_VERTICAL_MARGIN);
				view_data.m_line_numbers->setFont(m_font);
				view_data.m_line_numbers->setZValue(2);
				view_data.m_line_numbers->setDefaultTextColor(get_display_color(DisplayColors::TEXT));

				update_line_spacing(view_data.m_line_numbers);

				view_data.m_line_numbers->setVisible(false);
			}
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
			// Update the fonts
			m_font.setWordSpacing(m_display_config.m_word_spacing);
			m_font.setLetterSpacing(QFont::AbsoluteSpacing, m_display_config.m_letter_spacing);

			for (auto& current_view_pair : m_view_data_lookup)
			{
				ViewData& view_data = current_view_pair.second;
				view_data.m_text_item->setFont(m_font);
				view_data.m_line_numbers->setFont(m_font);
			}
		}

		void update_line_spacing(QGraphicsTextItem* item)
		{
			QTextBlockFormat format;
			format.setLineHeight(m_display_config.m_line_spacing, QTextBlockFormat::LineDistanceHeight);

			QTextCursor doc_cursor(item->document());
			doc_cursor.select(QTextCursor::Document);
			doc_cursor.mergeBlockFormat(format);
		}

		// Cache for PICT resources used in terminals
		QMap<int, QString> m_pict_path_cache;
		
		// Store data per-view
		std::unordered_map<int, ViewData> m_view_data_lookup;
		std::vector<int> m_view_data_free_list;
		int m_view_id_counter = 0;

		// Cache for display configuration data (font, spacing, etc.)
		QFont m_font;
		DisplayConfig m_display_config;
	};

	DisplaySystem::~DisplaySystem() = default;

	DisplaySystem::ViewID DisplaySystem::register_graphics_view(QGraphicsView* graphics_view)
	{
		// Check if we can reuse an entry from the free list
		ViewID new_view_id;
		const bool is_free_list_empty = m_internal->m_view_data_free_list.empty();
		new_view_id.m_id = is_free_list_empty ? (m_internal->m_view_id_counter++) : m_internal->m_view_data_free_list.back();

		ViewData& new_view_data = m_internal->m_view_data_lookup[new_view_id.m_id];
		if (!is_free_list_empty)
		{
			m_internal->m_view_data_free_list.pop_back();
		}
		else
		{
			// We created a new view, initialize it
			m_internal->init_view(new_view_data);
		}

		graphics_view->setScene(&new_view_data.m_scene);
		graphics_view->show();

		return new_view_id;
	}

	void DisplaySystem::release_graphics_view(const ViewID& view_id, QGraphicsView* graphics_view)
	{
		assert(view_id.is_valid());
		auto view_it = m_internal->m_view_data_lookup.find(view_id.get_id());
		assert(view_it != m_internal->m_view_data_lookup.end());

		// Make sure we didn't try to release more than once
		assert(std::find(m_internal->m_view_data_free_list.begin(), m_internal->m_view_data_free_list.end(), view_id.get_id()) == m_internal->m_view_data_free_list.end());
		m_internal->m_view_data_free_list.push_back(view_id.get_id());

		graphics_view->setScene(nullptr);

		clear_display(view_id);
	}

	void DisplaySystem::update_resources(const QString& resource_path)
	{
		// Clear the pict cache
		m_internal->m_pict_path_cache.clear();

		for (auto& current_view_pair : m_internal->m_view_data_lookup)
		{
			ViewData& current_view_data = current_view_pair.second;

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

	int DisplaySystem::update_display(const ViewID& view_id, const DisplayData& data)
	{
		auto view_it = m_internal->m_view_data_lookup.find(view_id.get_id());
		assert(view_it != m_internal->m_view_data_lookup.end());
		ViewData& selected_view = view_it->second;

		if (data != selected_view.m_display_data)
		{
			// Update the display contents
			update_image(selected_view, data);
			update_text(selected_view, data);

			// Update the screen type and alignment
			selected_view.m_display_data.m_screen_type = data.m_screen_type;
			selected_view.m_display_data.m_alignment = data.m_alignment;
		}

		// Return how many lines the current text contains
		return get_text_document_line_count(selected_view.m_text_item->document());
	}

	void DisplaySystem::clear_display(const ViewID& view_id)
	{
		auto view_it = m_internal->m_view_data_lookup.find(view_id.get_id());
		assert(view_it != m_internal->m_view_data_lookup.end());
		
		// Hide the contents
		ViewData& selected_view = view_it->second;
		selected_view.m_image_item->setVisible(false);
		selected_view.m_text_item->setVisible(false);
	}

	const DisplaySystem::DisplayConfig& DisplaySystem::get_display_config() const { return m_internal->m_display_config; }

	void DisplaySystem::set_display_config(const DisplayConfig& config) 
	{	
		m_internal->apply_display_config(config); 

		for (auto& current_view_pair : m_internal->m_view_data_lookup)
		{
			ViewData& current_view_data = current_view_pair.second;
			update_text(current_view_data, current_view_data.m_display_data);
		}
	}

	const QMap<int, QString>& DisplaySystem::get_pict_cache() const { return m_internal->m_pict_path_cache; }

	int DisplaySystem::get_page_count(int line_count)
	{
		return ceil(double(line_count) / double(SCREEN_MAX_LINES));
	}

	DisplaySystem::DisplaySystem(AppCore& core)
		: m_internal(std::make_unique<Internal>())
	{
	}

	void DisplaySystem::update_image(ViewData& view, const DisplayData& data)
	{
		switch (data.m_screen_type)
		{
		case Terminal::ScreenType::NONE:
		case Terminal::ScreenType::INFORMATION:
		case Terminal::ScreenType::CHECKPOINT:
		case Terminal::ScreenType::TAG:
			// Hide image, text only!
			view.m_image_item->setVisible(false);
			return;
		case Terminal::ScreenType::STATIC:
			view.m_image_item->setVisible(true);
			view.m_image_item->setPixmap(QPixmap(STATIC_SCREEN_IMAGE));
			view.m_image_item->setPos(0, 0);
			view.m_display_data.m_resource_id = -1;
			return;
		default:
			view.m_image_item->setVisible(true);
			break;
		}

		if ((data.m_resource_id != view.m_display_data.m_resource_id) || (data.m_resource_id == -1))
		{
			// Load the new resource (code is only reachable when the screen has an image)
			view.m_image_item->setPixmap(m_internal->get_pict(data.m_resource_id));
			view.m_display_data.m_resource_id = data.m_resource_id;
		}

		// Reposition according to the type
		const QRectF image_rect = view.m_image_item->boundingRect();
		const qreal vertical_offset = (TERMINAL_HEIGHT - image_rect.height()) * 0.5;
		qreal horizontal_offset = 0;

		switch (data.m_screen_type)
		{
		case Terminal::ScreenType::LOGON:
		case Terminal::ScreenType::LOGOFF:
			// Move to center
			horizontal_offset = (TERMINAL_WIDTH - image_rect.width()) * 0.5;
			break;
		case Terminal::ScreenType::PICT:
		{	
			// Check alignment
			switch (data.m_alignment)
			{
			case Terminal::ScreenAlignment::LEFT:
				horizontal_offset = ((TERMINAL_WIDTH * 0.5) - image_rect.width()) * 0.5;
				break;
			case Terminal::ScreenAlignment::CENTER:
				horizontal_offset = (TERMINAL_WIDTH - image_rect.width()) * 0.5;
				break;
			case Terminal::ScreenAlignment::RIGHT:
				horizontal_offset = ((TERMINAL_WIDTH * 1.5) - image_rect.width()) * 0.5;
				break;
			}
		}
			break;
		}

		view.m_image_item->setPos(horizontal_offset, vertical_offset);
	}

	void DisplaySystem::update_text(ViewData& view, const DisplayData& data)
	{
		switch (data.m_screen_type)
		{
		case Terminal::ScreenType::PICT:
		{
			if (data.m_alignment == Terminal::ScreenAlignment::CENTER)
			{
				// Do not display centered PICT text!
				view.m_text_item->setVisible(false);
				view.m_line_numbers->setVisible(false);
				return;
			}
			else
			{
				view.m_text_item->setVisible(true);
				view.m_line_numbers->setVisible(m_internal->m_display_config.m_show_line_numbers);
			}
		}
		break;
		case Terminal::ScreenType::TAG:
		case Terminal::ScreenType::STATIC:
			view.m_text_item->setVisible(false);
			view.m_line_numbers->setVisible(false);
			return;
		default:
			view.m_text_item->setVisible(true); 
			view.m_line_numbers->setVisible(m_internal->m_display_config.m_show_line_numbers);
			break;
		}

		// Use HTML to handle formatting
		view.m_display_data.m_text = data.m_text;
		view.m_text_item->setHtml(view.m_display_data.m_text);

		// Set alignment
		{
			QTextOption text_option = view.m_text_item->document()->defaultTextOption();
			switch (data.m_screen_type)
			{
			case Terminal::ScreenType::LOGON:
			case Terminal::ScreenType::LOGOFF:
				text_option.setAlignment(Qt::AlignCenter);
				view.m_line_numbers->setVisible(false); // Also make sure we don't show line numbers for these screens
				break;
			default:
				text_option.setAlignment(Qt::AlignLeft);
				break;
			}
			view.m_text_item->document()->setDefaultTextOption(text_option); 
		}

		QPointF line_number_position(m_internal->m_display_config.m_horizontal_margin / 2, m_internal->m_display_config.m_vertical_margin);
		QTextOption line_number_text_option = view.m_line_numbers->document()->defaultTextOption();
		line_number_text_option.setAlignment(Qt::AlignRight);

		QPointF text_position;

		// Reposition according to type
		switch (data.m_screen_type)
		{
		case Terminal::ScreenType::LOGON:
		case Terminal::ScreenType::LOGOFF:
		{
			// Use maximum available width (text will be centered horizontally)
			view.m_text_item->setTextWidth(TERMINAL_WIDTH - (2 * m_internal->m_display_config.m_horizontal_margin));

			// Set center, below image
			const QRectF image_rect = view.m_image_item->boundingRect();
			text_position = QPointF(m_internal->m_display_config.m_horizontal_margin, (TERMINAL_HEIGHT + image_rect.height()) * 0.5);
		}
			break;
		case Terminal::ScreenType::INFORMATION:
			// NOTE: values taken from AO source code, will need to make it adaptable
			text_position = QPointF(m_internal->m_display_config.m_horizontal_margin, m_internal->m_display_config.m_vertical_margin);
			view.m_text_item->setTextWidth(-1); // No need to limit width, line wrapping is provided via custom logic
			break;
		case Terminal::ScreenType::PICT:
		case Terminal::ScreenType::CHECKPOINT:
		{
			// Check alignment (text must be opposite the image)
			switch (data.m_alignment)
			{
			case Terminal::ScreenAlignment::LEFT:
				text_position.setX(TERMINAL_WIDTH * 0.5);
				line_number_position.setX(TERMINAL_WIDTH - LINE_NUMBER_RIGHT_OFFSET); // Make sure line numbers appear close to the text
				line_number_text_option.setAlignment(Qt::AlignLeft);
				break;
			case Terminal::ScreenAlignment::RIGHT:
				text_position.setX(TERMINAL_BORDER);
				line_number_position.setX(LINE_NUMBER_LEFT_OFFSET);
				break;
			}

			text_position.setY(TERMINAL_BORDER);
			line_number_position.setY(TERMINAL_BORDER);
			view.m_text_item->setTextWidth(-1); // No need to limit width, line wrapping is provided via custom logic
			break;
		}
		}

		view.m_line_numbers->setPos(line_number_position);
		view.m_line_numbers->document()->setDefaultTextOption(line_number_text_option);

		view.m_text_item->setPos(text_position);

		// Have to do this to make sure the line spacing is correct (might be redundant?)
		m_internal->update_line_spacing(view.m_text_item);
		m_internal->update_line_spacing(view.m_line_numbers);
	}
}