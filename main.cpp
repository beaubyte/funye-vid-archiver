#include <iostream>
#include <cstdlib>
#include <dpp/dpp.h>

std::string BOT_TOKEN;
std::string version = "0.0.2";

int main() {
        if (const char* env_token = std::getenv("BOT_TOKEN"))
        {
            BOT_TOKEN = env_token;
        } else
        {
            std::cerr << "BOT_TOKEN not set" << std::endl;
            return 1;
        }
    // create bot object with token
    dpp::cluster bot(BOT_TOKEN);
    // set discord bot intents
    bot.intents = dpp::i_default_intents | dpp::i_message_content;
    bot.on_log(dpp::utility::cout_logger());

    bot.on_slashcommand([&bot](const dpp::slashcommand_t& event) {
        if (event.command.get_command_name() == "version") {
            event.reply("current version: " + version);
        }
        if (event.command.get_command_name() == "getlinks")
        {
            dpp::snowflake channel_id = event.command.channel_id;

            bot.messages_get(channel_id, 0,0,0,20, [event](const dpp::confirmation_callback_t& callback)
            {
                std::string response;
                auto messages = callback.get<dpp::message_map>();
                response += "Retrieved " + std::to_string(messages.size()) + " messages\n";
                for (const auto& [msg_id, msg] : messages)
                {
                    if (msg.content.starts_with("https://"))
                    {
                        response += (msg.author.username + " : '" + msg.content + "' posted on: " + std::to_string(msg.get_creation_time()) + "\n");
                    }
                }
                event.reply(response);
            });
        }
    });

    bot.on_ready([&bot](const dpp::ready_t& event) {
        // list of commands in the bot
        if (dpp::run_once<struct register_bot_commands>()) {
            bot.global_command_create(dpp::slashcommand("version", "prints the version of funye-vid-archiver", bot.me.id));
            bot.global_command_create(dpp::slashcommand("getlinks", "gets messages that include website links", bot.me.id));
        }
    });

    bot.start(dpp::st_wait);
}