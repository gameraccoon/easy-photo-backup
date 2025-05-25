#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use std::env;
use std::process::exit;
use eframe::egui;
use eframe::egui::{Align, FontId, Layout, RichText};

const AFTER_CONFIRMATION_CLOSE_TIME: std::time::Duration = std::time::Duration::from_secs(1);

enum ExitCodes {
    Confirmed = 1,
    Aborted = 2,
    ErrorNoCodeProvided = 3,
}

fn main() -> eframe::Result {
    let args: Vec<String> = env::args().collect();

    if args.len() < 2 {
        println!("Wrong usage of the app, missing the first argument");
        exit(ExitCodes::ErrorNoCodeProvided as i32);
    }

    let code_to_display = args[1].clone();

    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default().with_inner_size([320.0, 200.0]),
        centered: true,
        run_and_return: true,
        ..Default::default()
    };
    eframe::run_native(
        "Pairing request",
        options,
        Box::new(|_cc| Ok(Box::<ServerDigitGui>::new(ServerDigitGui{
            code_to_display,
            code_entering_is_done: false,
            user_confirmation: None,
        }))),
    )
}

struct UserConfirmation {
    is_successful: bool,
    confirmation_time: std::time::Instant,
}

#[derive(Default)]
struct ServerDigitGui {
    code_to_display: String,
    code_entering_is_done: bool,
    user_confirmation: Option<UserConfirmation>,
}

impl eframe::App for ServerDigitGui {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        egui::CentralPanel::default().show(ctx, |ui| {
            ui.vertical_centered(|ui| {
                if !self.code_entering_is_done {
                    ui.add_space(20.0);
                    ui.heading("Enter this code on the device you want to pair");
                    ui.label(RichText::new(self.code_to_display.as_str()).font(FontId::proportional(40.0)));
                    ui.add_space(30.0);
                    if ui.button("Done").clicked() {
                        self.code_entering_is_done = true;
                    }
                } else {
                    match &self.user_confirmation {
                        None => {
                            ui.add_space(20.0);
                            ui.heading("Has the other device confirmed pairing");

                            ui.columns(2, |columns| {
                                columns[0].with_layout(Layout::right_to_left(Align::Center), |ui| {
                                    if ui.button("Yes").clicked() {
                                        self.user_confirmation = Some(UserConfirmation {
                                            is_successful: true,
                                            confirmation_time: std::time::Instant::now(),
                                        });
                                    }
                                });
                                columns[1].with_layout(Layout::left_to_right(Align::Center), |ui| {
                                    if ui.button("No").clicked() {
                                        self.user_confirmation = Some(UserConfirmation {
                                            is_successful: false,
                                            confirmation_time: std::time::Instant::now(),
                                        });
                                    }
                                });
                            });
                        }
                        Some(user_confirmation) => {
                            if user_confirmation.confirmation_time.elapsed() < AFTER_CONFIRMATION_CLOSE_TIME {
                                ui.centered_and_justified(|ui| {
                                    ui.heading(if user_confirmation.is_successful { "Pairing confirmed" } else { "Aborting" });
                                });
                            } else {
                                exit(if user_confirmation.is_successful { ExitCodes::Confirmed } else { ExitCodes::Aborted } as i32)
                            }
                        }
                    }
                }
            });
        });

        if ctx.input(|i| i.viewport().close_requested()) {
            exit(ExitCodes::Aborted as i32);
        }
    }
}
