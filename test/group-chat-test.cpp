#include "fixture.h"
#include <glib/gstrfuncs.h>
#include <fmt/format.h>

class GroupChatTest: public CommTest {
protected:
    const int32_t     groupId             = 700;
    const int64_t     groupChatId         = 7000;
    const std::string groupChatTitle      = "Title";
    const std::string groupChatPurpleName = "chat" + std::to_string(groupChatId);

    void loginWithBasicGroup();
};

void GroupChatTest::loginWithBasicGroup()
{
    login(
        {
            make_object<updateBasicGroup>(make_object<basicGroup>(
                groupId, 2, make_object<chatMemberStatusMember>(), true, 0
            )),
            make_object<updateNewChat>(makeChat(
                groupChatId, make_object<chatTypeBasicGroup>(groupId), groupChatTitle, nullptr, 0, 0, 0
            )),
            makeUpdateChatListMain(groupChatId)
        },
        make_object<users>(),
        make_object<chats>(std::vector<int64_t>(1, groupChatId)),
        {}, {},
        {
            std::make_unique<ConnectionSetStateEvent>(connection, PURPLE_CONNECTED),
            std::make_unique<AddChatEvent>(
                groupChatPurpleName, groupChatTitle, account, nullptr, nullptr
            ),
            std::make_unique<AccountSetAliasEvent>(account, selfFirstName + " " + selfLastName),
            std::make_unique<ShowAccountEvent>(account)
        }
    );

    tgl.verifyRequest(getBasicGroupFullInfo(groupId));
}

TEST_F(GroupChatTest, AddBasicGroupChatAtLogin)
{
    loginWithBasicGroup();
}

TEST_F(GroupChatTest, BasicGroupChatAppearsAfterLogin)
{
    login();

    tgl.update(make_object<updateBasicGroup>(make_object<basicGroup>(
        groupId, 2, make_object<chatMemberStatusMember>(), true, 0
    )));
    tgl.verifyNoRequests();

    tgl.update(make_object<updateNewChat>(makeChat(
        groupChatId, make_object<chatTypeBasicGroup>(groupId), groupChatTitle, nullptr, 0, 0, 0
    )));
    prpl.verifyNoEvents();
    tgl.verifyNoRequests();
    tgl.update(makeUpdateChatListMain(groupChatId));

    tgl.verifyRequest(getBasicGroupFullInfo(groupId));
    prpl.verifyEvents(AddChatEvent(
        groupChatPurpleName, groupChatTitle, account, NULL, NULL
    ));
}

TEST_F(GroupChatTest, ExistingBasicGroupChatAtLogin)
{
    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup((groupChatPurpleName).c_str()));
    purple_blist_add_chat(purple_chat_new(account, groupChatTitle.c_str(), components), NULL, NULL);
    prpl.discardEvents();

    login(
        {
            make_object<updateBasicGroup>(make_object<basicGroup>(
                groupId, 2, make_object<chatMemberStatusMember>(), true, 0
            )),
            make_object<updateNewChat>(makeChat(
                groupChatId, make_object<chatTypeBasicGroup>(groupId), groupChatTitle, nullptr, 0, 0, 0
            )),
            makeUpdateChatListMain(groupChatId)
        },
        make_object<users>(),
        make_object<chats>(std::vector<int64_t>(1, groupChatId))
    );

    tgl.verifyRequest(getBasicGroupFullInfo(groupId));
}

TEST_F(GroupChatTest, BasicGroupReceiveTextAndReply)
{
    constexpr int32_t date[]       = {12345, 123456};
    constexpr int64_t messageId[]  = {10000, 10001};
    constexpr int     purpleChatId = 1;
    loginWithBasicGroup();

    tgl.update(standardUpdateUserNoPhone(0));
    tgl.update(make_object<updateNewMessage>(
        makeMessage(messageId[0], userIds[0], groupChatId, false, date[0], makeTextMessage("Hello"))
    ));
    tgl.verifyRequest(viewMessages(
        groupChatId,
        {messageId[0]},
        true
    ));
    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, purpleChatId, groupChatPurpleName, groupChatTitle),
        ServGotChatEvent(connection, purpleChatId, userFirstNames[0] + " " + userLastNames[0],
                         "Hello", PURPLE_MESSAGE_RECV, date[0])
    );

    object_ptr<message> reply = makeMessage(messageId[1], selfId, groupChatId, true, date[1],
                                            makeTextMessage("Reply"));
    reply->reply_to_message_id_ = messageId[0];
    tgl.update(make_object<updateNewMessage>(std::move(reply)));
    tgl.verifyRequests({
        make_object<viewMessages>(
            groupChatId,
            std::vector<int64_t>(1, messageId[1]),
            true
        ),
        make_object<getMessage>(groupChatId, messageId[0])
    });
    prpl.verifyNoEvents();

    tgl.reply(make_object<ok>());
    tgl.reply(makeMessage(messageId[0], userIds[0], groupChatId, false, date[0], makeTextMessage("Hello")));
    prpl.verifyEvents(ConversationWriteEvent(
        groupChatPurpleName, selfFirstName + " " + selfLastName,
        fmt::format(replyPattern, userFirstNames[0] + " " + userLastNames[0], "Hello", "Reply"),
        PURPLE_MESSAGE_SEND, date[1]
    ));
}

TEST_F(GroupChatTest, BasicGroupReceivePhoto)
{
    const int32_t date         = 12345;
    const int64_t messageId    = 10001;
    const int32_t fileId       = 1234;
    constexpr int purpleChatId = 1;
    loginWithBasicGroup();

    tgl.update(standardUpdateUserNoPhone(0));
    tgl.update(make_object<updateNewMessage>(makeMessage(
        messageId, userIds[0], groupChatId, false, date,
        make_object<messagePhoto>(
            makePhotoRemote(fileId, 10000, 640, 480),
            make_object<formattedText>("photo", std::vector<object_ptr<textEntity>>()),
            false
        )
    )));
    tgl.verifyRequests({
        make_object<viewMessages>(groupChatId, std::vector<int64_t>(1, messageId), true),
        make_object<downloadFile>(fileId, 1, 0, 0, true)
    });
    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, purpleChatId, groupChatPurpleName, groupChatTitle),
        ServGotChatEvent(connection, purpleChatId, userFirstNames[0] + " " + userLastNames[0], "photo",
                         PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(groupChatPurpleName, "",
                               userFirstNames[0] + " " + userLastNames[0] + ": Downloading photo",
                               PURPLE_MESSAGE_SYSTEM, date)
    );

    tgl.reply(make_object<ok>());
    tgl.reply(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    ));
    prpl.verifyEvents(ServGotChatEvent(
        connection, 1, userFirstNames[0] + " " + userLastNames[0], "<img src=\"file:///path\">",
        (PurpleMessageFlags)(PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_IMAGES), date
    ));
}

TEST_F(GroupChatTest, ExistingBasicGroupReceiveMessageAtLogin_WithMemberList_RemoveGroupMemberFromBuddies)
{
    constexpr int64_t messageId    = 10001;
    constexpr int32_t date         = 12345;
    constexpr int     purpleChatId = 2;

    GHashTable *table = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(table, (char *)"id", g_strdup((groupChatPurpleName).c_str()));
    purple_blist_add_chat(purple_chat_new(account, groupChatTitle.c_str(), table), NULL, NULL);

    // Pre-add one of two group members as a contact
    purple_blist_add_buddy(purple_buddy_new(account, purpleUserName(0).c_str(),
                                            (userFirstNames[0] + " " + userLastNames[0]).c_str()),
                           NULL, NULL, NULL);
    prpl.discardEvents();

    login(
        {
            // Private chat with the contact
            standardUpdateUser(0),
            standardPrivateChat(0, make_object<chatListMain>()),

            make_object<updateBasicGroup>(make_object<basicGroup>(
                groupId, 2, make_object<chatMemberStatusMember>(), true, 0
            )),
            make_object<updateNewChat>(makeChat(
                groupChatId, make_object<chatTypeBasicGroup>(groupId), groupChatTitle, nullptr, 0, 0, 0
            )),
            makeUpdateChatListMain(groupChatId),
            make_object<updateNewMessage>(
                makeMessage(messageId, userIds[0], groupChatId, false, date, makeTextMessage("Hello"))
            )
        },
        make_object<users>(1, std::vector<int32_t>(1, userIds[0])),
        make_object<chats>(std::vector<int64_t>(1, groupChatId)),
        {
            // chat title is wrong at this point because libpurple doesn't find the chat in contact
            // list while the contact is not online, and thus has no way of knowing the chat alias.
            // Real libpurple works like that and our mock version mirrors the behaviour.
            std::make_unique<ServGotJoinedChatEvent>(connection, purpleChatId, groupChatPurpleName,
                                                     groupChatPurpleName),
            // Now chat title is corrected
            std::make_unique<ConvSetTitleEvent>(groupChatPurpleName, groupChatTitle),
            std::make_unique<ServGotChatEvent>(connection, purpleChatId, userFirstNames[0] + " " + userLastNames[0],
                                               "Hello", PURPLE_MESSAGE_RECV, date)
        },
        {make_object<viewMessages>(groupChatId, std::vector<int64_t>(1, messageId), true)},
        {
            std::make_unique<ConnectionSetStateEvent>(connection, PURPLE_CONNECTED),
            std::make_unique<UserStatusEvent>(account, purpleUserName(0), PURPLE_STATUS_OFFLINE),
            std::make_unique<AccountSetAliasEvent>(account, selfFirstName + " " + selfLastName),
            std::make_unique<ShowAccountEvent>(account)
        }
    );

    tgl.verifyRequest(getBasicGroupFullInfo(groupId));
    tgl.update(standardUpdateUser(1));
    std::vector<object_ptr<chatMember>> members;
    members.push_back(make_object<chatMember>(
        userIds[0],
        userIds[1],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    members.push_back(make_object<chatMember>(
        userIds[1],
        userIds[1],
        0,
        make_object<chatMemberStatusCreator>("", true),
        nullptr
    ));
    members.push_back(make_object<chatMember>(
        selfId,
        userIds[1],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    tgl.reply(make_object<basicGroupFullInfo>(
        "basic group",
        userIds[1],
        std::move(members),
        ""
    ));

    // One code path: adding chat users upon receiving getBasicGroupFullInfo reply, because the chat
    // window is already open due to the received message
    prpl.verifyEvents(
        ChatSetTopicEvent(groupChatPurpleName, "basic group", ""),
        ChatClearUsersEvent(groupChatPurpleName),
        ChatAddUserEvent(
            groupChatPurpleName,
            // This user is in our contact list so his libpurple user name is used
            purpleUserName(0),
            "", PURPLE_CBFLAGS_NONE, false
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
            // This user is not in our contact list so first/last name is used
            userFirstNames[1] + " " + userLastNames[1],
            "", PURPLE_CBFLAGS_FOUNDER, false
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
            // This is us (with + to match account name)
            "+" + selfPhoneNumber,
            "", PURPLE_CBFLAGS_NONE, false
        )
    );

    // Now remove the group member that is in buddy list from the buddy list
    PurpleBuddy *buddy = purple_find_buddy(account, purpleUserName(0).c_str());
    ASSERT_NE(nullptr, buddy);
    purple_blist_remove_buddy(buddy);
    prpl.discardEvents();
    // Normally there should be pluginInfo().remove_buddy to simulate user removing the buddy by hand.
    // But skip it and just say chat is magically removed from chatListMain
    tgl.update(make_object<updateChatChatList>(chatIds[0], nullptr));

    prpl.verifyEvents(
        ChatSetTopicEvent(groupChatPurpleName, "basic group", ""),
        ChatClearUsersEvent(groupChatPurpleName),
        ChatAddUserEvent(
            groupChatPurpleName,
            // This user is no longer in our contact list so first/last name is used
            userFirstNames[0] + " " + userLastNames[0],
            "", PURPLE_CBFLAGS_NONE, false
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
            userFirstNames[1] + " " + userLastNames[1],
            "", PURPLE_CBFLAGS_FOUNDER, false
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
            "+" + selfPhoneNumber,
            "", PURPLE_CBFLAGS_NONE, false
        )
    );
}

TEST_F(GroupChatTest, SendMessageWithMemberList)
{
    constexpr int64_t messageId    = 10001;
    constexpr int32_t date         = 12345;
    constexpr int     purpleChatId = 1;

    loginWithBasicGroup();

    tgl.update(standardUpdateUserNoPhone(0));
    tgl.update(standardUpdateUserNoPhone(1));
    std::vector<object_ptr<chatMember>> members;
    members.push_back(make_object<chatMember>(
        userIds[0],
        userIds[1],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    members.push_back(make_object<chatMember>(
        userIds[1],
        userIds[1],
        0,
        make_object<chatMemberStatusCreator>("", true),
        nullptr
    ));
    members.push_back(make_object<chatMember>(
        selfId,
        userIds[1],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    tgl.reply(make_object<basicGroupFullInfo>(
        "basic group",
        userIds[1],
        std::move(members),
        ""
    ));

    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup((groupChatPurpleName).c_str()));
    pluginInfo().join_chat(connection, components);
    g_hash_table_destroy(components);

    // Another code path: adding chat users upon opening chat, with basicGroupFullInfo before that
    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, purpleChatId, groupChatPurpleName, groupChatTitle),
        ChatSetTopicEvent(groupChatPurpleName, "basic group", ""),
        ChatClearUsersEvent(groupChatPurpleName),
        ChatAddUserEvent(
            groupChatPurpleName,
            userFirstNames[0] + " " + userLastNames[0],
            "", PURPLE_CBFLAGS_NONE, false
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
            userFirstNames[1] + " " + userLastNames[1],
            "", PURPLE_CBFLAGS_FOUNDER, false
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
            // This is us (with + to match account name)
            "+" + selfPhoneNumber,
            "", PURPLE_CBFLAGS_NONE, false
        ),
        PresentConversationEvent(groupChatPurpleName)
    );
    tgl.verifyNoRequests();

    ASSERT_EQ(0, pluginInfo().chat_send(connection, purpleChatId, "message", PURPLE_MESSAGE_SEND));
    tgl.verifyRequest(sendMessage(
        groupChatId,
        0,
        nullptr,
        nullptr,
        make_object<inputMessageText>(
            make_object<formattedText>("message", std::vector<object_ptr<textEntity>>()),
            false,
            false
        )
    ));
    prpl.verifyNoEvents();

    tgl.update(make_object<updateNewMessage>(
        makeMessage(messageId, selfId, groupChatId, true, date, makeTextMessage("message"))
    ));
    tgl.verifyRequest(viewMessages(
        groupChatId,
        {messageId},
        true
    ));
    prpl.verifyEvents(ConversationWriteEvent(
        groupChatPurpleName, selfFirstName + " " + selfLastName,
        "message", PURPLE_MESSAGE_SEND, date
    ));
}

TEST_F(GroupChatTest, JoinBasicGroupByInviteLink)
{
    const char *const LINK         = "https://t.me/joinchat/";
    constexpr int     purpleChatId = 1;
    login();

    // As if "Add chat" function in pidgin was used
    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup(""));
    g_hash_table_insert(components, (char *)"link", g_strdup(LINK));

    PurpleChat *chat = purple_chat_new(account, "old chat", components);
    purple_blist_add_chat(chat, NULL, NULL);
    prpl.discardEvents();

    // And now that chat is being joined
    pluginInfo().join_chat(connection, components);
    prpl.verifyNoEvents();

    uint64_t joinRequestId = tgl.verifyRequest(joinChatByInviteLink(LINK));

    // Success
    tgl.update(make_object<updateBasicGroup>(make_object<basicGroup>(
        groupId, 2, make_object<chatMemberStatusMember>(), true, 0
    )));
    prpl.verifyNoEvents();
    tgl.verifyNoRequests();

    auto chatUpdate = make_object<updateNewChat>(makeChat(
        groupChatId, make_object<chatTypeBasicGroup>(groupId), groupChatTitle, nullptr, 0, 0, 0
    ));
    chatUpdate->chat_->chat_list_ = make_object<chatListMain>();
    tgl.update(std::move(chatUpdate));
    // Chat is added, list of members requested
    prpl.verifyEvents(AddChatEvent(
        groupChatPurpleName, groupChatTitle, account, NULL, NULL
    ));
    uint64_t groupInfoRequestId = tgl.verifyRequest(getBasicGroupFullInfo(groupId));

    // There will always be this "message" about joining the group
    tgl.update(make_object<updateNewMessage>(
        makeMessage(1, selfId, groupChatId, true, 12345, make_object<messageChatJoinByLink>())
    ));
    uint64_t viewMessagesRequestId = tgl.verifyRequest(viewMessages(
        groupChatId,
        {1},
        true
    ));

    // The message is shown in chat conversation
    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, purpleChatId, groupChatPurpleName, groupChatTitle),
        ConversationWriteEvent(groupChatPurpleName, "",
                               selfFirstName + " " + selfLastName + ": " +
                               "Received unsupported message type messageChatJoinByLink",
                               PURPLE_MESSAGE_SYSTEM, 12345)
    );

    // Now reply to join group - original chat is removed
    tgl.reply(joinRequestId, makeChat(
        groupChatId, make_object<chatTypeBasicGroup>(groupId), groupChatTitle, nullptr, 0, 0, 0
    ));
    prpl.verifyEvents(RemoveChatEvent("", LINK));

    // Replying to group full info request with list of members
    tgl.update(standardUpdateUser(0));
    tgl.update(standardUpdateUser(1));
    std::vector<object_ptr<chatMember>> members;
    members.push_back(make_object<chatMember>(
        userIds[0],
        userIds[1],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    members.push_back(make_object<chatMember>(
        userIds[1],
        userIds[1],
        0,
        make_object<chatMemberStatusCreator>("", true),
        nullptr
    ));
    members.push_back(make_object<chatMember>(
        selfId,
        userIds[1],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    tgl.reply(groupInfoRequestId, make_object<basicGroupFullInfo>(
        "basic group",
        userIds[1],
        std::move(members),
        ""
    ));

    prpl.verifyEvents(
        ChatSetTopicEvent(groupChatPurpleName, "basic group", ""),
        ChatClearUsersEvent(groupChatPurpleName),
        ChatAddUserEvent(
            groupChatPurpleName,
            // This user is not in our contact list so first/last name is used
            userFirstNames[0] + " " + userLastNames[0],
            "", PURPLE_CBFLAGS_NONE, false
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
            // This user is not in our contact list so first/last name is used
            userFirstNames[1] + " " + userLastNames[1],
            "", PURPLE_CBFLAGS_FOUNDER, false
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
            // This is us (with + to match account name)
            "+" + selfPhoneNumber,
            "", PURPLE_CBFLAGS_NONE, false
        )
    );

    tgl.reply(viewMessagesRequestId, make_object<ok>());
}

TEST_F(GroupChatTest, GroupRenamed)
{
    const int64_t messageId    = 1;
    const int32_t date         = 1234;
    constexpr int purpleChatId = 1;

    loginWithBasicGroup();
    tgl.update(make_object<updateChatTitle>(groupChatId, "New Title"));
    prpl.verifyEvents(AliasChatEvent(groupChatPurpleName, "New Title"));

    tgl.update(standardUpdateUserNoPhone(0));
    tgl.update(make_object<updateNewMessage>(
        makeMessage(
            messageId, userIds[0], groupChatId, false, date,
            make_object<messageChatChangeTitle>("New Title")
        )
    ));
    tgl.verifyRequest(viewMessages(
        groupChatId,
        {messageId},
        true
    ));
    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, purpleChatId, groupChatPurpleName, "New Title"),
        ConversationWriteEvent(
            groupChatPurpleName, "",
            userFirstNames[0] + " " + userLastNames[0] + " changed group name to New Title",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );
}

TEST_F(GroupChatTest, AddContactByGroupChatName)
{
    loginWithBasicGroup();

    // We get to know about a non-contact because it's in group members
    tgl.update(standardUpdateUser(1));
    std::vector<object_ptr<chatMember>> members;
    members.push_back(make_object<chatMember>(
        userIds[0],
        userIds[1],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    members.push_back(make_object<chatMember>(
        userIds[1],
        userIds[1],
        0,
        make_object<chatMemberStatusCreator>("", true),
        nullptr
    ));
    members.push_back(make_object<chatMember>(
        selfId,
        userIds[1],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    tgl.reply(make_object<basicGroupFullInfo>(
        "basic group",
        userIds[1],
        std::move(members),
        ""
    ));

    // Adding him to contact list from group chat members
    PurpleBuddy *buddy = purple_buddy_new(account, (userFirstNames[1] + " " + userLastNames[1]).c_str(), "");
    purple_blist_add_buddy(buddy, NULL, &standardPurpleGroup, NULL);
    prpl.discardEvents();
    pluginInfo().add_buddy(connection, buddy, &standardPurpleGroup);

    // The buddy is deleted right away, to be replaced later
    prpl.verifyEvents(RemoveBuddyEvent(account, userFirstNames[1] + " " + userLastNames[1]));
    tgl.verifyRequest(addContact(make_object<contact>(
        "", userFirstNames[1], userLastNames[1], "", userIds[1]
    ), true));

    tgl.reply(make_object<ok>());
    tgl.verifyRequest(createPrivateChat(userIds[1], false));
    // The rest is tested elsewhere
}

TEST_F(GroupChatTest, CreateRemoveBasicGroupInAnotherClient)
{
    constexpr int32_t date[]       = {12345, 123456};
    constexpr int64_t messageId[]  = {10000, 10001};
    constexpr int     purpleChatId = 2;
    loginWithOneContact();

    tgl.update(make_object<updateBasicGroup>(make_object<basicGroup>(
        groupId, 2, make_object<chatMemberStatusCreator>("", true), true, 0
    )));
    tgl.verifyNoRequests();

    tgl.update(make_object<updateNewChat>(makeChat(
        groupChatId, make_object<chatTypeBasicGroup>(groupId), groupChatTitle, nullptr, 0, 0, 0
    )));
    prpl.verifyNoEvents();
    tgl.verifyNoRequests();

    std::vector<int32_t> members = {selfId, userIds[0]};
    tgl.update(make_object<updateNewMessage>(
        makeMessage(messageId[0], selfId, groupChatId, true, date[0],
                    make_object<messageBasicGroupChatCreate>(groupChatTitle, std::move(members)))
    ));
    tgl.verifyRequest(viewMessages(groupChatId, {messageId[0]}, true));
    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, purpleChatId, groupChatPurpleName, groupChatPurpleName),
        ConvSetTitleEvent(groupChatPurpleName, groupChatTitle),
        ConversationWriteEvent(groupChatPurpleName, "",
                               selfFirstName + " " + selfLastName +
                               ": Received unsupported message type messageBasicGroupChatCreate",
                               PURPLE_MESSAGE_SYSTEM, date[0])
    );

    tgl.update(makeUpdateChatListMain(groupChatId));
    tgl.verifyRequest(getBasicGroupFullInfo(groupId));
    prpl.verifyEvents(AddChatEvent(
        groupChatPurpleName, groupChatTitle, account, NULL, NULL
    ));

    tgl.update(make_object<updateBasicGroupFullInfo>(
        groupId,
        make_object<basicGroupFullInfo>("basic group", selfId, std::vector<object_ptr<chatMember>>(), "")
    ));
    tgl.update(make_object<updateBasicGroup>(make_object<basicGroup>(
        groupId, 0,
        make_object<chatMemberStatusCreator>("", false), // We are no longer group member
        true, 0
    )));

    tgl.update(make_object<updateNewMessage>(
        makeMessage(messageId[1], selfId, groupChatId, true, date[1],
                    make_object<messageChatDeleteMember>(selfId))
    ));
    tgl.verifyRequest(viewMessages(groupChatId, {messageId[1]}, true));
    prpl.verifyEvents(
        ChatSetTopicEvent(groupChatPurpleName, "basic group", ""),
        ChatClearUsersEvent(groupChatPurpleName),
        ConversationWriteEvent(groupChatPurpleName, "",
                               selfFirstName + " " + selfLastName +
                               ": Received unsupported message type messageChatDeleteMember",
                               PURPLE_MESSAGE_SYSTEM, date[1])
    );

    tgl.update(make_object<updateChatChatList>(groupChatId, nullptr));
    prpl.verifyEvents(RemoveChatEvent(groupChatPurpleName, ""));

    // There is a check that fails message sending if we are not a group member
    ASSERT_LT(pluginInfo().chat_send(connection, purpleChatId, "message", PURPLE_MESSAGE_SEND), 0);
}

TEST_F(GroupChatTest, DeleteBasicGroup_Creator)
{
    loginWithBasicGroup();
    tgl.update(make_object<updateBasicGroup>(make_object<basicGroup>(
        groupId, 2, make_object<chatMemberStatusCreator>("", true), true, 0
    )));
    PurpleChat *chat = purple_blist_find_chat(account, groupChatPurpleName.c_str());
    ASSERT_NE(nullptr, chat);
    GList *actions = pluginInfo().blist_node_menu(&chat->node);

    nodeMenuAction(&chat->node, actions, "Delete group");
    prpl.verifyEvents(RequestActionEvent(connection, account, NULL, NULL, 2));
    prpl.requestedAction("_No");
    tgl.verifyNoRequests();
    prpl.verifyNoEvents();

    nodeMenuAction(&chat->node, actions, "Delete group");
    prpl.verifyEvents(RequestActionEvent(connection, account, NULL, NULL, 2));
    prpl.requestedAction("_Yes");
    tgl.verifyRequests({
        make_object<leaveChat>(groupChatId),
        make_object<deleteChatHistory>(groupChatId, true, false)
    });

    g_list_free_full(actions, (GDestroyNotify)purple_menu_action_free);
}

TEST_F(GroupChatTest, DeleteBasicGroup_NonCreator)
{
    loginWithBasicGroup();
    PurpleChat *chat = purple_blist_find_chat(account, groupChatPurpleName.c_str());
    ASSERT_NE(nullptr, chat);
    GList *actions = pluginInfo().blist_node_menu(&chat->node);

    nodeMenuAction(&chat->node, actions, "Delete group");
    prpl.verifyNoEvents();
    tgl.verifyNoRequests();

    g_list_free_full(actions, (GDestroyNotify)purple_menu_action_free);
}

TEST_F(GroupChatTest, LeaveBasicGroup)
{
    loginWithBasicGroup();
    PurpleChat *chat = purple_blist_find_chat(account, groupChatPurpleName.c_str());
    ASSERT_NE(nullptr, chat);
    GList *actions = pluginInfo().blist_node_menu(&chat->node);

    nodeMenuAction(&chat->node, actions, "Leave group");
    prpl.verifyEvents(RequestActionEvent(connection, account, NULL, NULL, 2));
    prpl.requestedAction("_No");
    tgl.verifyNoRequests();
    prpl.verifyNoEvents();

    nodeMenuAction(&chat->node, actions, "Leave group");
    prpl.verifyEvents(RequestActionEvent(connection, account, NULL, NULL, 2));
    prpl.requestedAction("_Yes");
    tgl.verifyRequests({
        make_object<leaveChat>(groupChatId),
        make_object<deleteChatHistory>(groupChatId, true, false)
    });

    g_list_free_full(actions, (GDestroyNotify)purple_menu_action_free);
}

TEST_F(GroupChatTest, UsersWithSameName)
{
    constexpr int     purpleChatId = 1;

    loginWithBasicGroup();

    tgl.update(standardUpdateUserNoPhone(0));
    // Second group chat member - same name as the first one
    tgl.update(make_object<updateUser>(makeUser(
        userIds[1],
        userFirstNames[0],
        userLastNames[0],
        "",
        make_object<userStatusOffline>()
    )));

    std::vector<object_ptr<chatMember>> members;
    members.push_back(make_object<chatMember>(
        userIds[0],
        userIds[1],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    members.push_back(make_object<chatMember>(
        userIds[1],
        userIds[1],
        0,
        make_object<chatMemberStatusCreator>("", true),
        nullptr
    ));
    members.push_back(make_object<chatMember>(
        selfId,
        userIds[1],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    tgl.reply(make_object<basicGroupFullInfo>(
        "basic group",
        userIds[1],
        std::move(members),
        ""
    ));

    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup((groupChatPurpleName).c_str()));
    pluginInfo().join_chat(connection, components);
    g_hash_table_destroy(components);

    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, purpleChatId, groupChatPurpleName, groupChatTitle),
        ChatSetTopicEvent(groupChatPurpleName, "basic group", ""),
        ChatClearUsersEvent(groupChatPurpleName),
        ChatAddUserEvent(
            groupChatPurpleName,
            userFirstNames[0] + " " + userLastNames[0],
            "", PURPLE_CBFLAGS_NONE, false
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
            userFirstNames[0] + " " + userLastNames[0] + " #1",
            "", PURPLE_CBFLAGS_FOUNDER, false
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
            // This is us (with + to match account name)
            "+" + selfPhoneNumber,
            "", PURPLE_CBFLAGS_NONE, false
        ),
        PresentConversationEvent(groupChatPurpleName)
    );
    tgl.verifyNoRequests();
}

TEST_F(GroupChatTest, WriteToNonContact)
{
    constexpr int64_t echoMessageId[2] = {10, 11};
    constexpr int32_t echoDate[2]      = {123456, 123457};
    constexpr int     purpleChatId     = 1;

    // Login and get member list for the basic group
    loginWithBasicGroup();
    tgl.update(standardUpdateUserNoPhone(0));

    auto deletedUser = makeUser(userIds[1], "", "", "", nullptr);
    deletedUser->type_ = make_object<userTypeDeleted>();
    tgl.update(make_object<updateUser>(std::move(deletedUser)));

    std::vector<object_ptr<chatMember>> members;
    members.push_back(make_object<chatMember>(
        userIds[0],
        userIds[0],
        0,
        make_object<chatMemberStatusCreator>("", true),
        nullptr
    ));
    members.push_back(make_object<chatMember>(
        userIds[1],
        userIds[0],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    members.push_back(make_object<chatMember>(
        selfId,
        userIds[0],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    tgl.reply(make_object<basicGroupFullInfo>(
        "basic group",
        userIds[1],
        std::move(members),
        ""
    ));

    // Open chat
    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup((groupChatPurpleName).c_str()));
    pluginInfo().join_chat(connection, components);
    g_hash_table_destroy(components);

    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, purpleChatId, groupChatPurpleName, groupChatTitle),
        ChatSetTopicEvent(groupChatPurpleName, "basic group", ""),
        ChatClearUsersEvent(groupChatPurpleName),
        ChatAddUserEvent(
            groupChatPurpleName,
            userFirstNames[0] + " " + userLastNames[0],
            "", PURPLE_CBFLAGS_FOUNDER, false
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
            // This is us (with + to match account name)
            "+" + selfPhoneNumber,
            "", PURPLE_CBFLAGS_NONE, false
        ),
        PresentConversationEvent(groupChatPurpleName)
    );
    tgl.verifyNoRequests();

    // Send message to chat member
    ASSERT_EQ(0, pluginInfo().send_im(
        connection,
        (userFirstNames[0] + " " + userLastNames[0]).c_str(),
        "message",
        PURPLE_MESSAGE_SEND
    ));
    tgl.verifyRequest(createPrivateChat(userIds[0], false));

    tgl.update(standardPrivateChat(0));
    // Group chat conversation is open, and private chat for one of the members is updated,
    // so member list is updated just in case
    prpl.verifyEvents(
        ChatSetTopicEvent(groupChatPurpleName, "basic group", ""),
        ChatClearUsersEvent(groupChatPurpleName),
        ChatAddUserEvent(
            groupChatPurpleName,
            userFirstNames[0] + " " + userLastNames[0],
            "", PURPLE_CBFLAGS_FOUNDER, false
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
            // This is us (with + to match account name)
            "+" + selfPhoneNumber,
            "", PURPLE_CBFLAGS_NONE, false
        )
    );

    tgl.reply(makeChat(
        chatIds[0],
        make_object<chatTypePrivate>(userIds[0]),
        userFirstNames[0] + " " + userLastNames[0],
        nullptr, 0, 0, 0
    ));
    tgl.verifyRequest(sendMessage(
        chatIds[0],
        0,
        nullptr,
        nullptr,
        make_object<inputMessageText>(
            make_object<formattedText>("message", std::vector<object_ptr<textEntity>>()),
            false,
            false
        )
    ));

    // Our own message is sent to us
    tgl.update(make_object<updateNewMessage>(makeMessage(
        echoMessageId[0],
        selfId,
        chatIds[0],
        true,
        echoDate[0],
        makeTextMessage("message")
    )));
    tgl.verifyRequest(viewMessages(chatIds[0], {echoMessageId[0]}, true));
    prpl.verifyEvents(
        NewConversationEvent(
            PURPLE_CONV_TYPE_IM, account,
            // No buddy yet – display name is used as user name
            userFirstNames[0] + " " + userLastNames[0]
        ),
        ConversationWriteEvent(
            // ditto
            userFirstNames[0] + " " + userLastNames[0],
            selfFirstName + " " + selfLastName,
            "message",
            PURPLE_MESSAGE_SEND, echoDate[0]
        )
    );

    tgl.update(make_object<updateChatChatList>(chatIds[0], make_object<chatListMain>()));
    prpl.verifyEvents(
        AddBuddyEvent(
            purpleUserName(0),
            userFirstNames[0] + " " + userLastNames[0],
            account, NULL, NULL, NULL
        ),
        ConversationWriteEvent(
            userFirstNames[0] + " " + userLastNames[0], "",
            "Future messages in this conversation will be shown in a different tab",
            PURPLE_MESSAGE_SYSTEM, 0
        ),
        ChatSetTopicEvent(groupChatPurpleName, "basic group", ""),
        ChatClearUsersEvent(groupChatPurpleName),
        // This group member has become a libpurple buddy, so member username will now be changed
        // from display name to buddy username
        ChatAddUserEvent(
            groupChatPurpleName,
            purpleUserName(0),
            "", PURPLE_CBFLAGS_FOUNDER, false
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
            // This is us (with + to match account name)
            "+" + selfPhoneNumber,
            "", PURPLE_CBFLAGS_NONE, false
        )
    );

    ASSERT_EQ(0, pluginInfo().send_im(
        connection,
        (userFirstNames[0] + " " + userLastNames[0]).c_str(),
        "message2",
        PURPLE_MESSAGE_SEND
    ));
    tgl.verifyRequest(sendMessage(
        chatIds[0],
        0,
        nullptr,
        nullptr,
        make_object<inputMessageText>(
            make_object<formattedText>("message2", std::vector<object_ptr<textEntity>>()),
            false,
            false
        )
    ));
    // Our own message is sent to us
    tgl.update(make_object<updateNewMessage>(makeMessage(
        echoMessageId[1],
        selfId,
        chatIds[0],
        true,
        echoDate[1],
        makeTextMessage("message2")
    )));
    tgl.verifyRequest(viewMessages(chatIds[0], {echoMessageId[1]}, true));
    prpl.verifyEvents(
        NewConversationEvent(
            PURPLE_CONV_TYPE_IM, account,
            // Now there is buddy, so use user-id user name
            purpleUserName(0)
        ),
        ConversationWriteEvent(
            // same here
            purpleUserName(0),
            selfFirstName + " " + selfLastName,
            "message2",
            PURPLE_MESSAGE_SEND, echoDate[1]
        )
    );
}

TEST_F(GroupChatTest, Kick)
{
    constexpr int     purpleChatId = 1;
    loginWithBasicGroup();
    tgl.update(standardUpdateUserNoPhone(0));

    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup((groupChatPurpleName).c_str()));
    pluginInfo().join_chat(connection, components);
    g_hash_table_destroy(components);

    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, purpleChatId, groupChatPurpleName, groupChatTitle),
        PresentConversationEvent(groupChatPurpleName)
    );

    PurpleConversation *conv = purple_find_chat(connection, purpleChatId);
    ASSERT_NE(nullptr, conv);
    prpl.runCommand("kick", conv, {purpleUserName(1)});
    prpl.verifyEvents(ConversationWriteEvent(
        groupChatPurpleName, "",
        "Cannot kick user: User not found",
        PURPLE_MESSAGE_NO_LOG, 0
    ));

    prpl.runCommand("kick", conv, {userFirstNames[1] + " " + userLastNames[1]});
    prpl.verifyEvents(ConversationWriteEvent(
        groupChatPurpleName, "",
        "Cannot kick user: User not found",
        PURPLE_MESSAGE_NO_LOG, 0
    ));

    prpl.runCommand("kick", conv, {purpleUserName(0)});
    tgl.verifyRequest(setChatMemberStatus(
        groupChatId, userIds[0],
        make_object<chatMemberStatusLeft>()
    ));
    tgl.reply(make_object<error>(100, "error"));
    prpl.verifyEvents(ConversationWriteEvent(
        groupChatPurpleName, "",
        "Cannot kick user: code 100 (error)",
        PURPLE_MESSAGE_SYSTEM, 0
    ));

    prpl.runCommand("kick", conv, {userFirstNames[0] + " " + userLastNames[0]});
    tgl.verifyRequest(setChatMemberStatus(
        groupChatId, userIds[0],
        make_object<chatMemberStatusLeft>()
    ));
    tgl.reply(make_object<ok>());
}

TEST_F(GroupChatTest, Invite)
{
    constexpr int     purpleChatId = 1;
    loginWithBasicGroup();
    tgl.update(standardUpdateUserNoPhone(0));

    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup((groupChatPurpleName).c_str()));
    pluginInfo().join_chat(connection, components);
    g_hash_table_destroy(components);

    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, purpleChatId, groupChatPurpleName, groupChatTitle),
        PresentConversationEvent(groupChatPurpleName)
    );

    pluginInfo().chat_invite(
        connection, purpleChatId, NULL,
        purpleUserName(1).c_str()
    );
    prpl.verifyEvents(ConversationWriteEvent(
        groupChatPurpleName, "",
        "Cannot add user to group: User not found",
        PURPLE_MESSAGE_NO_LOG, 0
    ));

    pluginInfo().chat_invite(
        connection, purpleChatId, NULL,
        (userFirstNames[1] + " " + userLastNames[1]).c_str()
    );
    prpl.verifyEvents(ConversationWriteEvent(
        groupChatPurpleName, "",
        "Cannot add user to group: User not found",
        PURPLE_MESSAGE_NO_LOG, 0
    ));

    pluginInfo().chat_invite(
        connection, purpleChatId, NULL,
        purpleUserName(0).c_str()
    );
    tgl.verifyRequest(addChatMember(groupChatId, userIds[0], 0));

    tgl.reply(make_object<error>(100, "error"));
    prpl.verifyEvents(ConversationWriteEvent(
        groupChatPurpleName, "",
        "Cannot add user to group: code 100 (error)",
        PURPLE_MESSAGE_SYSTEM, 0
    ));

    pluginInfo().chat_invite(
        connection, purpleChatId, NULL,
        (userFirstNames[0] + " " + userLastNames[0]).c_str()
    );
    tgl.verifyRequest(addChatMember(groupChatId, userIds[0], 0));
    tgl.reply(make_object<ok>());
}

TEST_F(GroupChatTest, GetInviteLink)
{
    constexpr int     purpleChatId = 1;
    loginWithBasicGroup();

    tgl.reply(make_object<basicGroupFullInfo>(
        "basic group",
        userIds[1],
        std::vector<object_ptr<chatMember>>(),
        ""
    ));

    PurpleChat *chat = purple_blist_find_chat(account, groupChatPurpleName.c_str());
    ASSERT_NE(nullptr, chat);
    GList *actions = pluginInfo().blist_node_menu(&chat->node);

    nodeMenuAction(&chat->node, actions, "Show invite link");
    tgl.verifyRequest(generateChatInviteLink(groupChatId));

    tgl.reply(make_object<error>(100, "error"));
    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, purpleChatId, groupChatPurpleName, groupChatTitle),
        ChatSetTopicEvent(groupChatPurpleName, "basic group", ""),
        ChatClearUsersEvent(groupChatPurpleName),
        ConversationWriteEvent(
            groupChatPurpleName, "",
            "Cannot generate invite link: code 100 (error)",
            PURPLE_MESSAGE_SYSTEM, 0
        )
    );

    nodeMenuAction(&chat->node, actions, "Show invite link");
    tgl.verifyRequest(generateChatInviteLink(groupChatId));
    tgl.update(make_object<updateBasicGroupFullInfo>(
        groupChatId,
        make_object<basicGroupFullInfo>(
            "basic group",
            userIds[1],
            std::vector<object_ptr<chatMember>>(),
            "http://invite"
        )
    ));
    prpl.verifyNoEvents();

    tgl.reply(make_object<chatInviteLink>("http://invite"));
    prpl.verifyEvents(
        ConversationWriteEvent(
            groupChatPurpleName, "",
            "http://invite",
            PURPLE_MESSAGE_SYSTEM, 0
        )
    );

    g_list_free_full(actions, (GDestroyNotify)purple_menu_action_free);
}
