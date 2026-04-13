#include "MineSweeper.h"
#include <gtkmm/messagedialog.h>
#include <algorithm>
#include <iostream>

namespace {

const char* const kCss = R"(
#ms-root { background-color: #e8ecf2; }
#ms-root .ms-title { font-weight: 800; font-size: 15px; color: #1a2b4a; }
#ms-root .ms-muted { font-size: 12px; color: #3d4f6f; }
#ms-root button.cell {
  padding: 0; margin: 0; min-width: 0; min-height: 0;
  border-radius: 4px;
  border-width: 1px; border-style: solid;
  border-top-color: #ffffff; border-left-color: #ffffff;
  border-right-color: #9aa8bc; border-bottom-color: #9aa8bc;
  background-color: #eef2f8;
  color: #111111;
  font-weight: 700; font-size: 15px;
}
#ms-root button.cell.flat {
  border-radius: 2px;
  border-top-color: #b0bccf; border-left-color: #b0bccf;
  border-right-color: #eef2f8; border-bottom-color: #eef2f8;
  background-color: #d4dbe8;
}
#ms-root button.cell.mine-hit {
  background-color: #ffb3b3;
}
#ms-root button.primary {
  padding: 6px 14px;
  font-weight: 600;
  border-radius: 6px;
  background-color: #3a6ee8;
  color: #ffffff;
  border-width: 1px;
  border-style: solid;
  border-top-color: #6b9fff;
  border-left-color: #6b9fff;
  border-right-color: #1f4bb8;
  border-bottom-color: #1f4bb8;
}
#ms-root button.primary label { color: #ffffff; }
#ms-root radiobutton { font-size: 12px; color: #223355; }
)";

} // namespace

void MineSweeper::show_info(Gtk::Window& parent, const Glib::ustring& title,
                            const Glib::ustring& msg, Gtk::MessageType type) {
    Gtk::MessageDialog dlg(parent, msg, false, type, Gtk::BUTTONS_OK);
    dlg.set_title(title);
    dlg.run();
}

MineSweeper::MineSweeper() {
    set_title("扫雷");
    set_border_width(12);
    set_resizable(false);
    vbox_.set_name("ms-root");
    vbox_.set_spacing(10);

    css_ = Gtk::CssProvider::create();
    try {
        css_->load_from_data(kCss);
        if (auto scr = Gdk::Screen::get_default())
            Gtk::StyleContext::add_provider_for_screen(
                scr, css_, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    } catch (const Glib::Error& e) {
        std::cerr << "CSS: " << e.what() << "\n";
    }

    top_bar_.set_spacing(12);
    vbox_.pack_start(top_bar_, Gtk::PACK_SHRINK);

    auto* title = Gtk::manage(new Gtk::Label("扫雷"));
    title->get_style_context()->add_class("ms-title");
    title->set_halign(Gtk::ALIGN_START);
    top_bar_.pack_start(*title, Gtk::PACK_SHRINK);

    status_.set_halign(Gtk::ALIGN_FILL);
    status_.set_xalign(0);
    status_.get_style_context()->add_class("ms-muted");
    status_.set_line_wrap(true);
    top_bar_.pack_start(status_, Gtk::PACK_EXPAND_WIDGET);

    mines_label_.get_style_context()->add_class("ms-muted");
    top_bar_.pack_start(mines_label_, Gtk::PACK_SHRINK);

    auto* btn_new = Gtk::manage(new Gtk::Button("新游戏"));
    btn_new->get_style_context()->add_class("primary");
    btn_new->signal_clicked().connect(sigc::mem_fun(*this, &MineSweeper::on_new_game));
    top_bar_.pack_start(*btn_new, Gtk::PACK_SHRINK);

    diff_bar_.set_spacing(14);
    vbox_.pack_start(diff_bar_, Gtk::PACK_SHRINK);

    rb_easy_.set_label("初级 9×9 · 10 雷");
    rb_medium_.set_label("中级 16×16 · 40 雷");
    rb_hard_.set_label("高级 16×30 · 99 雷");
    rb_easy_.join_group(rb_medium_);
    rb_hard_.join_group(rb_medium_);
    rb_medium_.set_active(true);

    diff_bar_.pack_start(rb_easy_, Gtk::PACK_SHRINK);
    diff_bar_.pack_start(rb_medium_, Gtk::PACK_SHRINK);
    diff_bar_.pack_start(rb_hard_, Gtk::PACK_SHRINK);

    rb_easy_.signal_clicked().connect(sigc::mem_fun(*this, &MineSweeper::on_difficulty));
    rb_medium_.signal_clicked().connect(sigc::mem_fun(*this, &MineSweeper::on_difficulty));
    rb_hard_.signal_clicked().connect(sigc::mem_fun(*this, &MineSweeper::on_difficulty));

    vbox_.pack_start(field_, Gtk::PACK_SHRINK);
    field_.set_row_spacing(2);
    field_.set_column_spacing(2);

    add(vbox_);
    load_difficulty(Difficulty::Medium);
    rebuild_field();
    new_round_state();
    show_all();
    sync_window_size();

    status_.set_text("准备开始：点击「新游戏」开局，或切换难度后自动重置。");
}

MineSweeper::~MineSweeper() = default;

void MineSweeper::load_difficulty(Difficulty d) {
    difficulty_ = d;
    switch (d) {
        case Difficulty::Easy:
            rows_ = 9;
            cols_ = 9;
            mines_ = 10;
            break;
        case Difficulty::Medium:
            rows_ = 16;
            cols_ = 16;
            mines_ = 40;
            break;
        case Difficulty::Hard:
            rows_ = 16;
            cols_ = 30;
            mines_ = 99;
            break;
    }
    flags_left_ = mines_;
}

int MineSweeper::cell_px() const {
    if (difficulty_ == Difficulty::Hard)
        return 28;
    return 32;
}

void MineSweeper::sync_window_size() {
    show_all_children();
    Gtk::Requisition mn, nt;
    vbox_.get_preferred_size(mn, nt);
    const int b = get_border_width();
    resize(nt.width + 2 * b, nt.height + 2 * b);
}

void MineSweeper::rebuild_field() {
    for (int r = 0; r < static_cast<int>(buttons_.size()); ++r)
        for (int c = 0; c < static_cast<int>(buttons_[r].size()); ++c)
            if (buttons_[r][c])
                field_.remove(*buttons_[r][c]);
    buttons_.clear();
    buttons_.assign(rows_, std::vector<Gtk::Button*>(cols_, nullptr));

    const int px = cell_px();
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            auto* b = Gtk::manage(new Gtk::Button());
            b->set_relief(Gtk::RELIEF_NONE);
            b->set_size_request(px, px);
            b->set_label("");
            b->get_style_context()->add_class("cell");
            b->signal_button_press_event().connect(
                sigc::bind(sigc::mem_fun(*this, &MineSweeper::on_cell_button_press), r, c),
                false);
            field_.attach(*b, c, r);
            buttons_[r][c] = b;
        }
    }
    field_.show_all();
}

void MineSweeper::new_round_state() {
    grid_.assign(rows_, std::vector<Cell>(cols_));
    revealed_count_ = 0;
    game_over_ = false;
    game_won_ = false;
    flags_left_ = mines_;
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            if (!buttons_[r][c])
                continue;
            buttons_[r][c]->set_sensitive(true);
            buttons_[r][c]->set_label("");
            buttons_[r][c]->override_color(Gdk::RGBA("#111111"));
            auto ctx = buttons_[r][c]->get_style_context();
            ctx->remove_class("flat");
            ctx->remove_class("mine-hit");
        }
    }
    mines_label_.set_text("剩余旗标: " + std::to_string(flags_left_));
}

void MineSweeper::place_mines(int skip_r, int skip_c) {
    std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));
    int placed = 0;
    while (placed < mines_) {
        const int r = static_cast<int>(rng() % static_cast<unsigned>(rows_));
        const int c = static_cast<int>(rng() % static_cast<unsigned>(cols_));
        if (grid_[r][c].mine)
            continue;
        if (r == skip_r && c == skip_c)
            continue;
        grid_[r][c].mine = true;
        ++placed;
    }
    count_adjacent();
}

void MineSweeper::count_adjacent() {
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            if (grid_[r][c].mine) {
                grid_[r][c].adjacent = 0;
                continue;
            }
            int n = 0;
            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    if (dr == 0 && dc == 0)
                        continue;
                    const int nr = r + dr;
                    const int nc = c + dc;
                    if (nr >= 0 && nr < rows_ && nc >= 0 && nc < cols_ && grid_[nr][nc].mine)
                        ++n;
                }
            }
            grid_[r][c].adjacent = n;
        }
    }
}

bool MineSweeper::on_cell_button_press(GdkEventButton* ev, int r, int c) {
    if (game_over_ || game_won_)
        return true;
    if (ev->button == 3) {
        on_cell_right_click(r, c);
        return true;
    }
    if (ev->button == 1) {
        on_cell_click(r, c);
        return true;
    }
    return false;
}

void MineSweeper::on_cell_click(int r, int c) {
    if (game_over_ || game_won_ || grid_[r][c].revealed || grid_[r][c].flagged)
        return;

    if (revealed_count_ == 0) {
        place_mines(r, c);
        status_.set_text("游戏开始：雷区已生成（首格安全）。左键继续翻开，右键插旗。");
    }

    if (grid_[r][c].mine) {
        end_game(false);
        return;
    }
    reveal(r, c);
    check_win();
    refresh_cells();
}

void MineSweeper::on_cell_right_click(int r, int c) {
    if (game_over_ || game_won_ || grid_[r][c].revealed)
        return;
    toggle_flag(r, c);
    refresh_cells();
    check_win();
}

void MineSweeper::reveal(int r, int c) {
    if (r < 0 || r >= rows_ || c < 0 || c >= cols_)
        return;
    if (grid_[r][c].revealed || grid_[r][c].flagged)
        return;
    grid_[r][c].revealed = true;
    ++revealed_count_;
    if (grid_[r][c].adjacent == 0)
        expand_empty(r, c);
}

void MineSweeper::expand_empty(int r, int c) {
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0)
                continue;
            reveal(r + dr, c + dc);
        }
    }
}

void MineSweeper::toggle_flag(int r, int c) {
    if (grid_[r][c].flagged) {
        grid_[r][c].flagged = false;
        ++flags_left_;
    } else if (flags_left_ > 0) {
        grid_[r][c].flagged = true;
        --flags_left_;
    }
}

bool MineSweeper::all_mines_flagged_correct() const {
    int on = 0;
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            if (!grid_[r][c].flagged)
                continue;
            if (!grid_[r][c].mine)
                return false;
            ++on;
        }
    }
    return on == mines_;
}

void MineSweeper::check_win() {
    const int safe = rows_ * cols_ - mines_;
    if (revealed_count_ == safe) {
        end_game(true);
        return;
    }
    if (all_mines_flagged_correct()) {
        end_game(true);
    }
}

void MineSweeper::end_game(bool won) {
    game_over_ = true;
    game_won_ = won;
    if (won) {
        status_.set_text("恭喜：已通关！");
        for (int r = 0; r < rows_; ++r) {
            for (int c = 0; c < cols_; ++c) {
                if (grid_[r][c].mine && !grid_[r][c].flagged) {
                    grid_[r][c].flagged = true;
                    --flags_left_;
                }
            }
        }
    } else {
        status_.set_text("失败：踩到地雷。");
        for (int r = 0; r < rows_; ++r) {
            for (int c = 0; c < cols_; ++c) {
                if (grid_[r][c].mine && !grid_[r][c].flagged)
                    grid_[r][c].revealed = true;
            }
        }
    }
    refresh_cells();
    if (won)
        show_info(*this, "扫雷", "恭喜通关！", Gtk::MESSAGE_INFO);
    else
        show_info(*this, "扫雷", "游戏失败，再接再厉。", Gtk::MESSAGE_WARNING);
}

void MineSweeper::refresh_cells() {
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            auto* b = buttons_[r][c];
            if (!b)
                continue;
            auto ctx = b->get_style_context();
            const Cell& cell = grid_[r][c];

            if (game_over_ && !game_won_ && cell.mine) {
                b->set_sensitive(false);
                ctx->add_class("flat");
                ctx->add_class("mine-hit");
                b->set_label("*");
                b->override_color(Gdk::RGBA("#220000"));
                continue;
            }

            if (cell.revealed) {
                b->set_sensitive(false);
                ctx->add_class("flat");
                ctx->remove_class("mine-hit");
                if (cell.mine) {
                    b->set_label("*");
                    b->override_color(Gdk::RGBA("#880000"));
                } else if (cell.adjacent > 0) {
                    b->set_label(std::to_string(cell.adjacent));
                    switch (cell.adjacent) {
                        case 1: b->override_color(Gdk::RGBA("#0000ff")); break;
                        case 2: b->override_color(Gdk::RGBA("#00aa00")); break;
                        case 3: b->override_color(Gdk::RGBA("#ff0000")); break;
                        case 4: b->override_color(Gdk::RGBA("#000084")); break;
                        case 5: b->override_color(Gdk::RGBA("#840000")); break;
                        case 6: b->override_color(Gdk::RGBA("#008284")); break;
                        case 7: b->override_color(Gdk::RGBA("#000000")); break;
                        default: b->override_color(Gdk::RGBA("#666666")); break;
                    }
                } else {
                    b->set_label("");
                    b->override_color(Gdk::RGBA("#333333"));
                }
            } else if (cell.flagged) {
                b->set_sensitive(true);
                ctx->remove_class("flat");
                ctx->remove_class("mine-hit");
                b->set_label("!");
                b->override_color(Gdk::RGBA("#cc0000"));
            } else {
                b->set_sensitive(true);
                ctx->remove_class("flat");
                ctx->remove_class("mine-hit");
                b->set_label("");
                b->override_color(Gdk::RGBA("#111111"));
            }
        }
    }
    mines_label_.set_text("剩余旗标: " + std::to_string(flags_left_));
}

void MineSweeper::on_new_game() {
    new_round_state();
    status_.set_text("游戏已开始：左键翻开，右键插/拔旗标。");
    show_info(*this, "扫雷", "游戏已开始！\n左键翻开格子，右键标记或取消地雷。", Gtk::MESSAGE_INFO);
}

void MineSweeper::on_difficulty() {
    Difficulty d = Difficulty::Medium;
    if (rb_easy_.get_active())
        d = Difficulty::Easy;
    else if (rb_hard_.get_active())
        d = Difficulty::Hard;
    else if (rb_medium_.get_active())
        d = Difficulty::Medium;
    else
        return;
    load_difficulty(d);
    rebuild_field();
    new_round_state();
    refresh_cells();
    status_.set_text("难度已切换，棋盘已重置。点击「新游戏」或直接在格子上开局。");
    sync_window_size();
}
