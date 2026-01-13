#include <catch2/catch_test_macros.hpp>
#include "network/MessageTypes.hpp"
#include "network/Serialization.hpp"

using namespace tetris::net;

//"Network Level 0: serialize/deserialize all message types"
TEST_CASE("Network Level 0: serialize/deserialize all message types") {

    // Criação de exemplos de cada tipo de mensagem
    JoinRequest joinReq{"Alice"};
    JoinAccept joinAcc{1, "Welcome Alice"};
    StartGame start{GameMode::TimeAttack, 300, 0, 100};
    InputActionMessage inputMsg{1, 42, tetris::controller::InputAction::MoveLeft};
    BoardCellDTO cell{true, 3};
    BoardDTO board{10, 20, std::vector<BoardCellDTO>(200, cell)};
    PlayerStateDTO playerState{1, "Alice", board, 1000, 5, true};
    StateUpdate stateUpdate{50, {playerState}};
    MatchResult matchRes{100, 1, MatchOutcome::Win, 1500};
    ErrorMessage errorMsg{"Something went wrong"};

    // Colocando todos os tipos em Messages
    std::vector<Message> messages = {
        {MessageKind::JoinRequest, joinReq},
        {MessageKind::JoinAccept, joinAcc},
        {MessageKind::StartGame, start},
        {MessageKind::InputActionMessage, inputMsg},
        {MessageKind::StateUpdate, stateUpdate},
        {MessageKind::MatchResult, matchRes},
        {MessageKind::Error, errorMsg}
    };

    // Testa serialização e desserialização
    for (auto& msg : messages) {
        auto serialized = serialize(msg);  // std::string
        REQUIRE(!serialized.empty());

        // desserializar retorna std::optional<Message>
        auto deserializedOpt = deserialize(serialized);
        REQUIRE(deserializedOpt.has_value());       // garante que veio algo

        auto& deserialized = deserializedOpt.value(); // pega o Message de dentro
        REQUIRE(deserialized.kind == msg.kind);       // agora dá pra acessar kind
    }
}
