#pragma once

#include "base_env.h"
#include <cassert>
#include <string>
#include <vector>

namespace minizero::env::addikul {

const std::string kAddiKulName = "addikul";
const int kAddiKulBoardSize = 7;
const int kAddiKulNumPlayer = 2;
const int kAddiKulBoardArea = kAddiKulBoardSize * kAddiKulBoardSize;
const int kAddiKulPolicySize = kAddiKulBoardArea * kAddiKulBoardArea + 1; // from-to plus pass
const int kAddiKulPassActionID = kAddiKulPolicySize - 1;

class AddiKulAction : public BaseAction {
public:
    AddiKulAction() : BaseAction() {}
    AddiKulAction(int action_id, Player player) : BaseAction(action_id, player) {}
    AddiKulAction(const std::vector<std::string>& action_string_args);

    inline Player nextPlayer() const override { return getNextPlayer(getPlayer(), kAddiKulNumPlayer); }
    std::string toConsoleString() const override;

    inline bool isPass() const { return action_id_ == kAddiKulPassActionID; }
    int getFromPos(int board_size = kAddiKulBoardSize) const;
    int getDestPos(int board_size = kAddiKulBoardSize) const;
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
    float getReward() const override;
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const AddiKulAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline int getNumInputChannels() const override { return 4; }
    inline int getPolicySize() const override { return kAddiKulPolicySize; }
    std::string toString() const override;
    inline std::string name() const override { return kAddiKulName; }
    inline int getNumPlayer() const override { return kAddiKulNumPlayer; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return utils::getPositionByRotating(rotation, position, getBoardSize()); }
    int getRotateAction(int action_id, utils::Rotation rotation) const override;

private:
    Player eval() const;
    std::vector<AddiKulAction> getMovesForPlayer(Player player) const;
    inline int encodeAction(int from, int dest) const { return from * kAddiKulBoardArea + dest; }

    // Termination helpers
    // - Pass is only legal when the current player has no legal moves.
    // - Game ends after two consecutive passes (i.e., both sides had no legal move).
    // - A max ply safeguard prevents any accidental infinite loops.
    int consecutive_pass_ = 0;
    int ply_ = 0;
    static constexpr int kMaxPly = 512;

    std::vector<Player> board_;
    GamePair<int> capture_counts_;
};

class AddiKulEnvLoader : public BaseBoardEnvLoader<AddiKulAction, AddiKulEnv> {
public:
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline std::vector<float> getValue(const int pos) const { return {getReturn()}; }
    inline std::string name() const override { return kAddiKulName; }
    inline int getPolicySize() const override { return kAddiKulPolicySize; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return utils::getPositionByRotating(rotation, position, getBoardSize()); }
    int getRotateAction(int action_id, utils::Rotation rotation) const override;
};

} // namespace minizero::env::addikul