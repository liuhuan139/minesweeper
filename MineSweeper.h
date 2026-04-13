#ifndef MINESWEEPER_H
#define MINESWEEPER_H

#include <gtkmm.h>
#include <vector>
#include <random>
#include <ctime>

class MineSweeper : public Gtk::Window {
public:
    MineSweeper();
    ~MineSweeper() override;

private:
    enum class Difficulty { Easy, Medium, Hard };

    struct Cell {
        bool mine = false;
        bool revealed = false;
        bool flagged = false;
        int adjacent = 0;
    };

    int rows_ = 16;
    int cols_ = 16;
    int mines_ = 40;
    Difficulty difficulty_ = Difficulty::Medium;

    std::vector<std::vector<Cell>> grid_;
    std::vector<std::vector<Gtk::Button*>> buttons_;
    bool game_over_ = false;
    bool game_won_ = false;
    int revealed_count_ = 0;
    int flags_left_ = 40;

    Gtk::VBox vbox_;
    Gtk::Overlay overlay_;
    Gtk::Revealer toast_revealer_;
    Gtk::HBox toast_shell_;
    Gtk::Label toast_label_;
    Gtk::HBox top_bar_;
    Gtk::HBox diff_bar_;
    Gtk::Label status_;
    Gtk::Label mines_label_;
    Gtk::Grid field_;
    Gtk::RadioButton rb_easy_, rb_medium_, rb_hard_;
    Glib::RefPtr<Gtk::CssProvider> css_;
    sigc::connection toast_hide_conn_;

    void load_difficulty(Difficulty d);
    int cell_px() const;
    void sync_window_size();
    void rebuild_field();
    void new_round_state();
    void place_mines(int skip_r, int skip_c);
    void count_adjacent();
    void on_cell_click(int r, int c);
    void on_cell_right_click(int r, int c);
    bool on_cell_button_press(GdkEventButton* ev, int r, int c);
    void reveal(int r, int c);
    void expand_empty(int r, int c);
    void toggle_flag(int r, int c);
    void check_win();
    bool all_mines_flagged_correct() const;
    void end_game(bool won);
    void refresh_cells();
    void on_new_game();
    void on_difficulty();

    void show_toast(const Glib::ustring& msg, Gtk::MessageType type);
    bool on_toast_hide_timeout();
};

#endif
