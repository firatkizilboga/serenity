/*
 * Copyright (c) 2026, Fırat Kızılboğa <firatkizilboga11@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Function.h>
#include <LibGUI/Button.h>
#include <LibGUI/CheckBox.h>
#include <LibGUI/Event.h>
#include <LibGUI/Label.h>
#include <LibGUI/TextBox.h>
#include <LibGUI/Widget.h>

namespace Browser {

class FindInPageWidget final
    : public GUI::Widget {
    C_OBJECT(FindInPageWidget);

public:
    virtual ~FindInPageWidget() override = default;
    Function<void(String const&)> on_search;
    Function<void()> on_next;
    Function<void()> on_previous;
    Function<void()> on_close;

    void update_result_label(size_t current_match_index, Optional<size_t> const& total_match_count);
    bool is_match_case_enabled() const { return m_match_case_checkbox->is_checked(); }

private:
    FindInPageWidget();
    virtual void keydown_event(GUI::KeyEvent&) override;

    RefPtr<GUI::Button> m_close_button;
    RefPtr<GUI::Button> m_previous_button;
    RefPtr<GUI::Button> m_next_button;
    RefPtr<GUI::TextBox> m_search_textbox;
    RefPtr<GUI::CheckBox> m_match_case_checkbox;
    RefPtr<GUI::Label> m_result_label;
};
}
