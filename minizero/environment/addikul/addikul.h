#pragma once

#include "base_env.h"
#include <string>
#include <utility>
#include <vector>

namespace minizero::env::addikul {

const std::string kAddiKulName = "addikul";
const int kAddiKulNumPlayer = 2;
const int kAddiKulBoardSize = 7;
const int kAddiKulPiecesPerPlayer = 21;
const int kAddiKulMaxMoves = 300;

class AddiKulAction : public BaseAction {
public:
    AddiKulAction() : BaseAction(), board_size_(kAddiKulBoardSize) {}
    AddiKulAction(int action_id, Player player) : BaseAction(action_id, player), board_size_(kAddiKulBoardSize) {}
    AddiKulAction(const std::vector<std::string>& action_string_args, int board_size = minizero::config::env_board_size);

    inline Player nextPlayer() const override { return getNextPlayer(getPlayer(), kAddiKulNumPlayer); }
    std::string toConsoleString() const override;

    inline int getFromID(int board_size = minizero::config::env_board_size) const
    {
        return action_id_ / (board_size * board_size);
    }
    inline int getDestID(int board_size = minizero::config::env_board_size) const
    {
        return action_id_ % (board_size * board_size);
    }

private:
    std::string normalizeMoveString(const std::string& move) const;
    std::pair<std::string, std::string> splitMoveString(const std::string& move) const;
    int parsePosition(const std::string& coord, int board_size) const;

    int board_size_ = kAddiKulBoardSize;
};

class AddiKulEnv : public BaseBoardEnv<AddiKulAction> {
public:
    AddiKulEnv() : BaseBoardEnv<AddiKulAction>(kAddiKulBoardSize) { reset(); }

    void reset() override;
    bool act(const AddiKulAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<AddiKulAction> getLegalActions() const override;
    bool isLegalAction(const AddiKulAction& action) const override;
    bool isTerminal() const override;
    float getReward() const override { return 0.0f; }
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const AddiKulAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline int getNumInputChannels() const override { return 4; }
    inline int getNumActionFeatureChannels() const override { return 2; }
    inline int getPolicySize() const override { return getBoardSize() * getBoardSize() * getBoardSize() * getBoardSize(); }
    std::string toString() const override;
    std::string getGameResultString() const;
    inline std::string name() const override { return kAddiKulName; }
    inline int getNumPlayer() const override { return kAddiKulNumPlayer; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override
    {
        return utils::getPositionByRotating(rotation, position, getBoardSize());
    };
    int getRotateAction(int action_id, utils::Rotation rotation) const override;

private:
    struct MoveInfo {
        bool is_capture = false;
        int jumped_pos = -1;
    };

    Player evalWinner() const;
    Player getPlayerAtBoardPos(int pos) const;
    MoveInfo getMoveInfo(int from, int dest, Player player) const;
    bool isInBoard(int x, int y) const;
    bool isCaptureMove(int from, int dest, Player player, MoveInfo& info) const;
    bool isSimpleMove(int from, int dest, Player player) const;

    std::string getCoordinateString() const;

    std::vector<Player> board_;
    GamePair<int> captured_;
    int move_count = 0;
};

class AddiKulEnvLoader : public BaseBoardEnvLoader<AddiKulAction, AddiKulEnv> {
public:
    void loadFromEnvironment(const AddiKulEnv& env, const std::vector<std::vector<std::pair<std::string, std::string>>>& action_info_history = {}) override
    {
        BaseBoardEnvLoader<AddiKulAction, AddiKulEnv>::loadFromEnvironment(env, action_info_history);
        addTag("REASON", env.getGameResultString());
    }
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline std::vector<float> getValue(const int pos) const { return {getReturn()}; }
    inline std::string name() const override { return kAddiKulName; }
    inline int getPolicySize() const override { return getBoardSize() * getBoardSize() * getBoardSize() * getBoardSize(); }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override
    {
        return utils::getPositionByRotating(rotation, position, getBoardSize());
    };
    int getRotateAction(int action_id, utils::Rotation rotation) const override;
};

} // namespace minizero::env::addikul