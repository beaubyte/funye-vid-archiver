#include <iostream>
#include <cstdlib>
#include <random>
#include <regex>
#include <dpp/dpp.h>

std::string BOT_TOKEN;
dpp::snowflake ARCHIVE_CHANNEL_ID = 1208233639066730556; // i should make this an environment variable
std::string version = "0.0.6";

// discord is weird and has its own epoch date, this function converts from standard epoch to their one so i dont have to remember
dpp::snowflake calculate_snowflake_time(uint64_t timestamp)
{
    // braced initializer moment
    return {(timestamp - 1420070400000) << 22};
}

uint64_t get_random_time(uint64_t min, uint64_t max)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dis(min, max);
    return dis(gen);
}

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

        // archive command: archive x amount of messages with links
        if (event.command.get_command_name() == "archive")
        {
            event.thinking();
            dpp::snowflake channel_id = event.command.channel_id;
            bot.messages_get(channel_id, 0,0,0,std::get<int64_t>(event.get_parameter("num_messages")), [event](const dpp::confirmation_callback_t& callback)
            {
                dpp::message response_msg(ARCHIVE_CHANNEL_ID, "");
                if (callback.is_error())
                {
                    event.edit_response("Failed to fetch messages");
                }
                auto messages = callback.get<dpp::message_map>();
                // lists amount of messages retrieved and appends to response
                std::vector<std::string> urls;
                for (const auto& [msg_id, msg] : messages)
                {
                    // if message matches the URL regex, it appends the content and details to the response, and then downloads the video
                    const std::regex url_pattern(R"(^https?://[a-zA-Z0-9\-._~:/?#\[\]@!$&'()*+,;=%]+$)");
                    if (std::regex_match(msg.content, url_pattern)) // i should detect links better or command injection might be possible, i will update to use fork later
                    {
                        urls.push_back(msg.content);
                    }
                }
                if (urls.empty())
                {
                    event.edit_response("No valid links were found");
                }
                for (int i = 0; i < urls.size(); i++)
                {
                    // i should refactor this to be its own class
                    std::string yt_dlp_command = "yt-dlp -o output" + std::to_string(i) + ".mp4 --force-overwrite ";
                    yt_dlp_command += urls[i];
                    std::cout << yt_dlp_command << std::endl;
                    system(yt_dlp_command.c_str());
                    response_msg.add_file("output.mp4", dpp::utility::read_file("./output" + std::to_string(i) + ".mp4"));
                }
                std::cout << "Done with file downloads\n";
                event.edit_response(response_msg);
                // removes output files to prevent previous files from being uploaded if yt-dlp command fails
                system("rm output*");
            });
        }

        // pin archive command: archive provided link with description to pin archive channel
        if (event.command.get_command_name() == "pin_archive")
        {
            event.thinking();
            std::string video_url = std::get<std::string>(event.get_parameter("video_url"));
            std::string description = std::get<std::string>(event.get_parameter("description"));
            const std::regex url_pattern(R"(^https?://[a-zA-Z0-9\-._~:/?#\[\]@!$&'()*+,;=%]+$)");
            dpp::message pin_message(ARCHIVE_CHANNEL_ID, description);
            if (std::regex_match(video_url, url_pattern))
            {
                std::string yt_dlp_command = "yt-dlp -o output.mp4 -f 'mp4' --force-overwrite " + video_url;
                system(yt_dlp_command.c_str());
                pin_message.add_file(description + ".mp4", dpp::utility::read_file("output.mp4"));
                event.edit_response("sending video to <#" + std::to_string(ARCHIVE_CHANNEL_ID) + ">");
            } else
            {
                event.edit_response("archive failed: link is not valid");
            }
            if (std::filesystem::exists("output.mp4"))
            {
                bot.message_create(pin_message, [event, video_url](const dpp::confirmation_callback_t& callback)
                {
                    if (callback.is_error())
                    {
                        auto error = callback.get_error();
                        if (error.code == 40005)
                        {
                            event.edit_response("archive failed: file denied because it exceeds the size limit");
                        }
                    } else
                    {
                        event.edit_response("[video](" + video_url + ") sent to <#" + std::to_string(ARCHIVE_CHANNEL_ID) + ">");
                        system("rm output.mp4");
                    }
                });
            } else {
                event.edit_response("archive failed: video failed to download");
            }
        }

        // random command - will return a random message that contains a file from within the specified archive channel
        if (event.command.get_command_name() == "random")
        {
            event.thinking();
            dpp::message response_message;
            // create a week_start to check for messages
            dpp::snowflake week_start = get_random_time(1638277200000, uint64_t(std::chrono::system_clock::now)); // start of 2022
            // query discord for a vector of messages from within that time period (start of week to end of week)
            bot.messages_get(ARCHIVE_CHANNEL_ID, 0, week_start, week_start + 604800000, 20, [event](const dpp::confirmation_callback_t& callback)
            {
                if (callback.is_error())
                {
                    event.edit_response("Failed to fetch messages");
                }
                auto messages = callback.get<dpp::message_map>();
                for (const auto& [msg_id, msg] : messages)
                {
                    // if it contains messages with files, choose a random message to return
                    if (!msg.attachments.empty()){
                        event.edit_response(msg.get_url());
                        break;
                    }
                }
            });

            // otherwise, pick a new random week
        }
    });

    bot.on_ready([&bot](const dpp::ready_t& event) {
        // list of commands in the bot
        if (dpp::run_once<struct register_bot_commands>()) {
            bot.global_command_create(dpp::slashcommand("version", "prints the version of funye-vid-archiver", bot.me.id));
            bot.global_command_create(dpp::slashcommand("getlinks", "gets last 20 messages that include website links", bot.me.id));
            dpp::slashcommand archive("archive", "replies with the last x amount of links as downloaded mp4s", bot.me.id);
            archive.add_option(
                dpp::command_option(dpp::co_integer, "num_messages", "number of previous messages to archive", true));
            bot.global_command_create(archive);
            dpp::slashcommand pin_archive("pin_archive", "downloads provided link and sends it to pin archive", bot.me.id);
            pin_archive.add_option(
                dpp::command_option(dpp::co_string, "video_url", "video link to archive", true));
            pin_archive.add_option(
                dpp::command_option(dpp::co_string, "description", "description of video for finding later", true));
            bot.global_command_create(pin_archive);
            dpp::slashcommand random("random","returns a random pin_archive video",bot.me.id);
            bot.global_command_create(random);
        }
    });

    bot.start(dpp::st_wait);
}

