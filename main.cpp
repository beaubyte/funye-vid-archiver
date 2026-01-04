#include <iostream>
#include <cstdlib>
#include <dpp/dpp.h>

std::string BOT_TOKEN;
std::string version = "0.0.1";

int main() {
        if (const char* env_token = std::getenv("BOT_TOKEN"))
        {
            BOT_TOKEN = env_token;
        } else
        {
            std::cerr << "BOT_TOKEN not set" << std::endl;
            return 1;
        }
    dpp::cluster bot(BOT_TOKEN);

    bot.on_log(dpp::utility::cout_logger());

    bot.on_slashcommand([](const dpp::slashcommand_t& event) {
        if (event.command.get_command_name() == "version") {
            event.reply("current version: " + version);
        }
    });

    bot.on_ready([&bot](const dpp::ready_t& event) {
        // list of commands in the bot
        if (dpp::run_once<struct register_bot_commands>()) {
            bot.global_command_create(dpp::slashcommand("version", "prints the version of funye-vid-archiver", bot.me.id));
        }
    });

    bot.start(dpp::st_wait);
}