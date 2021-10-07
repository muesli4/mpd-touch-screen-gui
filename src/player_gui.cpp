#include <libwtk-sdl2/padding.hpp>

#include "player_gui.hpp"
#include "widget_util.hpp"

#ifndef ICONDIR
#define ICONDIR "./"
#endif

widget_ptr player_gui::make_shutdown_view()
{
    return vbox( { { false
                   , std::make_shared<text_button>("Shutdown", [&](){ _model.shutdown(); }) 
                   }
                 , { false
                   , std::make_shared<text_button>("Reboot", [&](){ _model.reboot(); }) 
                   }
                 }, 5, true);
}

player_gui::player_gui(SDL_Renderer * renderer, player_model & model, std::vector<std::string> & playlist, int current_song_pos, program_config const & cfg)
    : _renderer(renderer)
    , _model(model)
    , _cover_view_ptr(std::make_shared<cover_view>( [&](swipe_direction dir){ handle_cover_swipe_direction(dir); }
                                                  , [&](){ _model.toggle_pause(); }
                                                  ))
    , _playlist_view_ptr(std::make_shared<list_view>( playlist
                                                    , current_song_pos
                                                    , [&](std::size_t pos){ _model.play_position(pos); }
                                                    ))
    , _search_view_ptr(std::make_shared<search_view>( renderer
                                                    , cfg.on_screen_keyboard.size
                                                    , cfg.on_screen_keyboard.keys
                                                    , playlist
                                                    , [&](auto pos){ _model.play_position(pos); }
                                                    ))
    , _view_ptr(std::make_shared<notebook>(
          std::vector<widget_ptr>{ _cover_view_ptr
                                 , add_list_view_controls(renderer, _playlist_view_ptr, ICONDIR "jump_to_arrow.png", [=, &current_song_pos](){ _playlist_view_ptr->set_position(current_song_pos); })
                                 , _search_view_ptr
                                 , make_shutdown_view()
                                 }))
    , _random_button_ptr(make_enum_texture_button<bool, 2>( renderer
                                                          , false
                                                          , { ICONDIR "random_on.png"
                                                            , ICONDIR "random_off.png"
                                                            }
                                                          , [&](){ _model.toggle_random(); }
                                                          ))
    , _play_button_ptr(make_enum_texture_button<mpd_state, 4>( renderer
                                                             , MPD_STATE_UNKNOWN
                                                             , { ICONDIR "play.png"
                                                               , ICONDIR "play.png"
                                                               , ICONDIR "pause.png"
                                                               , ICONDIR "play.png"
                                                               }
                                                             , [&](){ _model.toggle_pause(); }
                                                             ))
    , _main_widget( box::orientation::HORIZONTAL
                  , { { false, pad_right(-5, pad( 5
                                                , vbox({ { false, make_texture_button( renderer
                                                                                     , ICONDIR "apps.png"
                                                                                     , [&](){ _view_ptr->set_page((_view_ptr->get_page() + 1) % 4);  }
                                                                                     ) }
                                                       , { false, _play_button_ptr }
                                                       , { false, _random_button_ptr }
                                                       }, 5, true)
                                                )) }
                    , { true, pad(5, _view_ptr) }
                    }
                  , 0
                  )
    // TODO add dir_unambig_factor_threshold from config
    , _ctx(renderer, { cfg.default_font, cfg.big_font }, _main_widget)
{
    _ctx.draw();
}

void player_gui::handle_cover_swipe_direction(swipe_direction dir)
{
    switch (dir)
    {
        case swipe_direction::UP:
            _model.inc_volume(5);
            break;
        case swipe_direction::DOWN:
            _model.dec_volume(5);
            break;
        case swipe_direction::RIGHT:
            _model.next_song();
            break;
        case swipe_direction::LEFT:
            _model.prev_song();
            break;
        default:
            break;
    }
}

void player_gui::on_cover_updated(std::string cover_path)
{
    _cover_view_ptr->set_cover(std::move(load_texture_from_image(_renderer, cover_path)));
}

void player_gui::on_cover_updated(std::string title, std::string artist, std::string album)
{
    _cover_view_ptr->set_cover(title, artist, album);
}

void player_gui::on_song_changed(unsigned int new_song_position)
{
    _playlist_view_ptr->set_highlight_position(new_song_position);
    _search_view_ptr->set_filtered_highlight_position(new_song_position);
}

void player_gui::on_playlist_changed(bool reset_position)
{
    _search_view_ptr->on_playlist_changed();

    if (reset_position)
    {
        _playlist_view_ptr->set_position(0);
    }
}

void player_gui::on_random_changed(bool random)
{
    _random_button_ptr->set_state(random);
}

void player_gui::on_playback_state_changed(mpd_state playback_state)
{
    _play_button_ptr->set_state(playback_state);
}

void player_gui::on_navigation_event(navigation_event const & ne)
{
    if (ne.type == navigation_event_type::NAVIGATION)
    {
        _ctx.navigate_selection(ne.nt);
    }
    else
    {
        _ctx.activate();
    }
}

void player_gui::on_other_event(SDL_Event const & e)
{
    _ctx.process_event(e);
}

void player_gui::on_draw_dirty_event()
{
    _ctx.draw_dirty();
}

