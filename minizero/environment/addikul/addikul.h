#pragma once

#include "base_env.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace minizero::env::addikul {

const std::string kAddiKulName = "addikul";
const int kAddiKulNumPlayer = 2;
const int kAddiKulBoardSize = 7;

class AddiKulAction : public BaseBoardAction<kAddiKulNumPlayer> {
public:
    AddiKulAction() : BaseBoardAction<kAddiKulNumPlayer>() {}
    AddiKulAction(int action_id, Player player) : BaseBoardAction<kAddiKulNumPlayer>(action_id, player) {}
    AddiKulAction(const std::vector<std::string>& action_string_args, int board_size = kAddiKulBoardSize);

    inline int getFromID(int board_size = kAddiKulBoardSize) const { return action_id_ / (board_size * board_size); }
    inline int getDestID(int board_size = kAddiKulBoardSize) const { return action_id_ % (board_size * board_size); }
    inline static int encode(int from, int dest, int board_size = kAddiKulBoardSize)
    {
        return from * board_size * board_size + dest;
    }

    std::string toConsoleString() const override;
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
    inline int getNumActionFeatureChannels() const override { return 2; }
    inline int getNumInputChannels() const override { return 4; }
    inline int getPolicySize() const override { return getBoardSize() * getBoardSize() * getBoardSize() * getBoardSize(); }
    std::string toString() const override;
    inline std::string name() const override { return kAddiKulName; }
    inline int getNumPlayer() const override { return kAddiKulNumPlayer; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override
    {
        return utils::getPositionByRotating(rotation, position, getBoardSize());
    }
    int getRotateAction(int action_id, utils::Rotation rotation) const override;

private:
    bool getLegalAppliedAction(const AddiKulAction& action, AddiKulAction& applied_action) const;
    bool isDirectLegalAction(const AddiKulAction& action) const;
    std::string getStateKey() const;
    void recordState();
    bool isDraw() const; // 3-fold repetition (used as termination condition)
    Player eval() const;
    std::vector<Player> board_;
    std::unordered_map<std::string, int> state_counts_;
};

class AddiKulEnvLoader : public BaseBoardEnvLoader<AddiKulAction, AddiKulEnv> {
public:
    AddiKulEnvLoader() { board_size_ = kAddiKulBoardSize; }

    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline std::vector<float> getValue(const int pos) const { return {getReturn()}; }
    inline std::string name() const override { return kAddiKulName; }
    inline int getPolicySize() const override
    {
        int board_size = (getBoardSize() > 0 ? getBoardSize() : kAddiKulBoardSize);
        return board_size * board_size * board_size * board_size;
    }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override
    {
        return utils::getPositionByRotating(rotation, position, getBoardSize());
    }
    int getRotateAction(int action_id, utils::Rotation rotation) const override;
};

} // namespace minizero::env::addikul