#include <iostream>
#include <cstdlib>
#include <dpp/dpp.h>

std::string BOT_TOKEN;
dpp::snowflake ARCHIVE_CHANNEL_ID = 1208233639066730556;
std::string version = "0.0.2";

int main()
{
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

    // when a slash command is given
    bot.on_slashcommand([&bot](const dpp::slashcommand_t& event)
    {
        // version command, prints version of the bot
        if (event.command.get_command_name() == "version") {
            event.reply("current version: " + version);
        }
        // getlinks command: gets last 20 messages with a link
        if (event.command.get_command_name() == "getlinks")
        {
            dpp::snowflake channel_id = event.command.channel_id;

            bot.messages_get(channel_id, 0,0,0,20, [event](const dpp::confirmation_callback_t& callback)
            {
                std::string response;
                auto messages = callback.get<dpp::message_map>();
                // lists amount of messages retrieved and appends to response
                response += "Retrieved " + std::to_string(messages.size()) + " messages\n";
                for (const auto& [msg_id, msg] : messages)
                {
                    // if link starts with https://, it appends the content and details to the response
                    if (msg.content.starts_with("https://")) // i should detect links better or ill feed bad stuff to yt-dlp
                    {
                        response += (msg.author.username + " : '" + msg.content + "' posted on: " + std::to_string(msg.get_creation_time()) + "\n");
                    }
                }
                event.reply(response);
            });
        }
        if (event.command.get_command_name() == "archive")
        {
            event.reply("Starting archive of messages to #:pushpin:-funye-vid-pin-archive-:pushpin:");
            dpp::snowflake channel_id = event.command.channel_id;
            dpp::message response_msg;
            bot.messages_get(channel_id, 0,0,0,20, [event, &response_msg](const dpp::confirmation_callback_t& callback)
            {
                auto messages = callback.get<dpp::message_map>();
                // lists amount of messages retrieved and appends to response
                for (const auto& [msg_id, msg] : messages)
                {
                    // if link starts with https://, it appends the content and details to the response, and then downloads the video
                    if (msg.content.starts_with("https://")) // i should detect links better or ill feed bad stuff to yt-dlp
                    {
                        std::string yt_dlp_command = "yt-dlp -o output.mp4 ";
                        yt_dlp_command += msg.content;
                        system(yt_dlp_command.c_str());
                        response_msg.add_file("output.mp4", msg.content);
                    }
                }
            });
            response_msg.channel_id = ARCHIVE_CHANNEL_ID;
            bot.message_create((response_msg), [](const dpp::confirmation_callback_t& callback)
            {
                if (callback.is_error())
                {
                    std::cout << "Error sending message: " << callback.get_error().message << std::endl;
                }
            });
        }
    });

    bot.on_ready([&bot](const dpp::ready_t& event) {
        // list of commands in the bot
        if (dpp::run_once<struct register_bot_commands>()) {
            bot.global_command_create(dpp::slashcommand("version", "prints the version of funye-vid-archiver", bot.me.id));
            bot.global_command_create(dpp::slashcommand("getlinks", "gets last 20 messages that include website links", bot.me.id));
            bot.global_command_create(dpp::slashcommand("archive", "replies with last 20 message with links and their downloaded videos", bot.me.id));
        }
    });

    bot.start(dpp::st_wait);
}