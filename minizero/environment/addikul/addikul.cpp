#include "addikul.h"
#include "random.h"
#include "sgf_loader.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace minizero::env::addikul {

using namespace minizero::utils;

AddiKulAction::AddiKulAction(const std::vector<std::string>& action_string_args) : BaseAction()
{
    assert(action_string_args.size() == 2);
    assert(action_string_args[0].size() == 1);
    player_ = charToPlayer(action_string_args[0][0]);
    if (action_string_args[1] == "pass") {
        action_id_ = kAddiKulPassActionID;
    } else {
        assert(action_string_args[1].size() == 4);
        int from = SGFLoader::boardCoordinateStringToActionID(action_string_args[1].substr(0, 2), kAddiKulBoardSize);
        int dest = SGFLoader::boardCoordinateStringToActionID(action_string_args[1].substr(2, 2), kAddiKulBoardSize);
        action_id_ = from * kAddiKulBoardArea + dest;
    }
}

std::string AddiKulAction::toConsoleString() const
{
    if (isPass()) { return "pass"; }
    std::ostringstream oss;
    oss << SGFLoader::actionIDToBoardCoordinateString(getFromPos(), kAddiKulBoardSize)
        << SGFLoader::actionIDToBoardCoordinateString(getDestPos(), kAddiKulBoardSize);
    return oss.str();
}

int AddiKulAction::getFromPos(int board_size) const
{
    return (isPass() ? -1 : action_id_ / (board_size * board_size));
}

int AddiKulAction::getDestPos(int board_size) const
{
    return (isPass() ? -1 : action_id_ % (board_size * board_size));
}

void AddiKulEnv::reset()
{
    turn_ = Player::kPlayer1;
    actions_.clear();
    board_.assign(kAddiKulBoardArea, Player::kPlayerNone);
    capture_counts_.reset();
    consecutive_pass_ = 0;
    ply_ = 0;

    for (int row = 0; row < kAddiKulBoardSize; ++row) {
        for (int col = 0; col < kAddiKulBoardSize; ++col) {
            int pos = row * kAddiKulBoardSize + col;
            if (row < 3) {
                board_[pos] = Player::kPlayer1;
            } else if (row > 3) {
                board_[pos] = Player::kPlayer2;
            }
        }
    }
}

bool AddiKulEnv::act(const AddiKulAction& action)
{
    if (!isLegalAction(action)) { return false; }
    actions_.push_back(action);

    // safeguard against accidental infinite games
    ++ply_;

    if (action.isPass()) {
        // pass is only legal when the current player has no moves
        ++consecutive_pass_;
        turn_ = action.nextPlayer();
        return true;
    } else {
        consecutive_pass_ = 0;

        int from = action.getFromPos(board_size_);
        int dest = action.getDestPos(board_size_);
        int from_row = from / board_size_;
        int from_col = from % board_size_;
        int dest_row = dest / board_size_;
        int dest_col = dest % board_size_;
        int mid_row = (from_row + dest_row) / 2;
        int mid_col = (from_col + dest_col) / 2;
        int mid = mid_row * board_size_ + mid_col;

        board_[from] = Player::kPlayerNone;

        // capture on jump (distance 2 in row or col or diag)
        if (std::abs(dest_row - from_row) == 2 || std::abs(dest_col - from_col) == 2) {
            if (board_[mid] != Player::kPlayerNone && board_[mid] != turn_) {
                board_[mid] = Player::kPlayerNone;
                capture_counts_.get(turn_) += 1;
            }
        }

        board_[dest] = action.getPlayer();
    }

    turn_ = action.nextPlayer();
    return true;
}

bool AddiKulEnv::act(const std::vector<std::string>& action_string_args)
{
    return act(AddiKulAction(action_string_args));
}

std::vector<AddiKulAction> AddiKulEnv::getLegalActions() const
{
    std::vector<AddiKulAction> actions = getMovesForPlayer(turn_);
    if (actions.empty()) { actions.emplace_back(kAddiKulPassActionID, turn_); }
    return actions;
}

std::vector<AddiKulAction> AddiKulEnv::getMovesForPlayer(Player player) const
{
    std::vector<AddiKulAction> actions;
    int forward = (player == Player::kPlayer1 ? 1 : -1);
    std::vector<std::pair<int, int>> directions = {{0, forward}, {1, 0}, {-1, 0}, {1, forward}, {-1, forward}};

    for (int pos = 0; pos < kAddiKulBoardArea; ++pos) {
        if (board_[pos] != player) { continue; }
        int row = pos / board_size_;
        int col = pos % board_size_;

        for (auto [dx, dy] : directions) {
            // step
            int ncol = col + dx;
            int nrow = row + dy;
            if (ncol < 0 || ncol >= board_size_ || nrow < 0 || nrow >= board_size_) { continue; }
            int dest = nrow * board_size_ + ncol;
            if (board_[dest] == Player::kPlayerNone) {
                actions.emplace_back(encodeAction(pos, dest), player);
            }

            // jump capture
            int jump_col = col + 2 * dx;
            int jump_row = row + 2 * dy;
            if (jump_col < 0 || jump_col >= board_size_ || jump_row < 0 || jump_row >= board_size_) { continue; }
            int over = (row + dy) * board_size_ + (col + dx);
            int jump_dest = jump_row * board_size_ + jump_col;
            if (board_[over] != Player::kPlayerNone && board_[over] != player && board_[jump_dest] == Player::kPlayerNone) {
                actions.emplace_back(encodeAction(pos, jump_dest), player);
            }
        }
    }
    return actions;
}

bool AddiKulEnv::isLegalAction(const AddiKulAction& action) const
{
    if (action.isPass()) { return getMovesForPlayer(turn_).empty(); }
    if (action.getActionID() < 0 || action.getActionID() >= kAddiKulPassActionID) { return false; }
    if (action.getPlayer() != turn_) { return false; }

    int from = action.getFromPos(board_size_);
    int dest = action.getDestPos(board_size_);
    if (from < 0 || from >= kAddiKulBoardArea || dest < 0 || dest >= kAddiKulBoardArea) { return false; }
    if (board_[from] != action.getPlayer() || board_[dest] != Player::kPlayerNone) { return false; }

    int from_row = from / board_size_;
    int from_col = from % board_size_;
    int dest_row = dest / board_size_;
    int dest_col = dest % board_size_;
    int dy = dest_row - from_row;
    int dx = dest_col - from_col;
    int forward = (turn_ == Player::kPlayer1 ? 1 : -1);

    // allowed step directions: forward, left, right, forward-diagonal
    std::vector<std::pair<int, int>> step_dirs = {{0, forward}, {1, 0}, {-1, 0}, {1, forward}, {-1, forward}};
    auto it = std::find(step_dirs.begin(), step_dirs.end(), std::make_pair(dx, dy));
    bool is_step = (std::abs(dx) <= 1 && std::abs(dy) <= 1);
    if (is_step) {
        if (it == step_dirs.end()) { return false; }
        return true;
    }

    // jump must be exactly 2 in allowed direction and jump over opponent
    if (std::abs(dx) > 2 || std::abs(dy) > 2) { return false; }
    if (dx % 2 != 0 || dy % 2 != 0) { return false; }
    std::pair<int, int> jump_dir(dx / 2, dy / 2);
    if (std::find(step_dirs.begin(), step_dirs.end(), jump_dir) == step_dirs.end()) { return false; }

    int mid_row = from_row + jump_dir.second;
    int mid_col = from_col + jump_dir.first;
    if (mid_row < 0 || mid_row >= board_size_ || mid_col < 0 || mid_col >= board_size_) { return false; }
    int mid = mid_row * board_size_ + mid_col;

    return (board_[mid] != Player::kPlayerNone && board_[mid] != turn_);
}

bool AddiKulEnv::isTerminal() const
{
    if (ply_ >= kMaxPly) { return true; }

    int p1_pieces = std::count(board_.begin(), board_.end(), Player::kPlayer1);
    int p2_pieces = std::count(board_.begin(), board_.end(), Player::kPlayer2);
    if (p1_pieces == 0 || p2_pieces == 0) { return true; }

    // End after two consecutive passes (both sides had no legal move)
    if (consecutive_pass_ >= 2) { return true; }

    return false;
}

float AddiKulEnv::getReward() const
{
    // Reward from current player-to-move perspective (matches getFeatures()).
    if (!isTerminal()) { return 0.0f; }
    Player w = eval();
    if (w == Player::kPlayerNone) { return 0.0f; }
    return (w == turn_ ? 1.0f : -1.0f);
}

float AddiKulEnv::getEvalScore(bool is_resign /*= false*/) const
{
    Player result = (is_resign ? getNextPlayer(turn_, kAddiKulNumPlayer) : eval());
    if (result == Player::kPlayer1) { return 1.0f; }
    if (result == Player::kPlayer2) { return -1.0f; }
    return 0.0f;
}

std::vector<float> AddiKulEnv::getFeatures(utils::Rotation rotation) const
{
    std::vector<float> features;
    for (int channel = 0; channel < getNumInputChannels(); ++channel) {
        for (int pos = 0; pos < kAddiKulBoardArea; ++pos) {
            int rotation_pos = getRotatePosition(pos, utils::reversed_rotation[static_cast<int>(rotation)]);
            if (channel == 0) {
                features.push_back(board_[rotation_pos] == turn_ ? 1.0f : 0.0f);
            } else if (channel == 1) {
                features.push_back(board_[rotation_pos] == getNextPlayer(turn_, kAddiKulNumPlayer) ? 1.0f : 0.0f);
            } else if (channel == 2) {
                features.push_back(turn_ == Player::kPlayer1 ? 1.0f : 0.0f);
            } else {
                features.push_back(turn_ == Player::kPlayer2 ? 1.0f : 0.0f);
            }
        }
    }
    return features;
}

std::vector<float> AddiKulEnv::getActionFeatures(const AddiKulAction& action, utils::Rotation rotation) const
{
    std::vector<float> action_features(kAddiKulPolicySize, 0.0f);
    action_features[getRotateAction(action.getActionID(), rotation)] = 1.0f;
    return action_features;
}

std::string AddiKulEnv::toString() const
{
    std::ostringstream oss;
    oss << "   A  B  C  D  E  F  G" << std::endl;
    for (int row = kAddiKulBoardSize - 1; row >= 0; --row) {
        oss << row + 1 << " ";
        for (int col = 0; col < kAddiKulBoardSize; ++col) {
            Player p = board_[row * kAddiKulBoardSize + col];
            if (p == Player::kPlayerNone) {
                oss << " . ";
            } else if (p == Player::kPlayer1) {
                oss << " O ";
            } else {
                oss << " X ";
            }
        }
        oss << " " << row + 1 << std::endl;
    }
    oss << "   A  B  C  D  E  F  G" << std::endl;
    return oss.str();
}

int AddiKulEnv::getRotateAction(int action_id, utils::Rotation rotation) const
{
    if (action_id == kAddiKulPassActionID) { return action_id; }
    int from = action_id / kAddiKulBoardArea;
    int dest = action_id % kAddiKulBoardArea;
    int rotated_from = getRotatePosition(from, rotation);
    int rotated_dest = getRotatePosition(dest, rotation);
    return encodeAction(rotated_from, rotated_dest);
}

Player AddiKulEnv::eval() const
{
    int p1_pieces = std::count(board_.begin(), board_.end(), Player::kPlayer1);
    int p2_pieces = std::count(board_.begin(), board_.end(), Player::kPlayer2);
    if (p2_pieces == 0) { return Player::kPlayer1; }
    if (p1_pieces == 0) { return Player::kPlayer2; }

    // When both players consecutively had no legal move (two passes),
    // decide winner by capture count. Also applies if max ply triggers.
    if (consecutive_pass_ < 2 && ply_ < kMaxPly) { return Player::kPlayerNone; }

    int p1_capture = capture_counts_.get(Player::kPlayer1);
    int p2_capture = capture_counts_.get(Player::kPlayer2);
    if (p1_capture > p2_capture) { return Player::kPlayer1; }
    if (p2_capture > p1_capture) { return Player::kPlayer2; }
    return Player::kPlayerNone;
}

std::vector<float> AddiKulEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation) const
{
    std::vector<float> action_features(kAddiKulPolicySize, 0.0f);
    int action_id;
    if (pos < static_cast<int>(action_pairs_.size())) {
        const AddiKulAction& action = action_pairs_[pos].first;
        action_id = getRotateAction(action.getActionID(), rotation);
    } else {
        action_id = utils::Random::randInt() % action_features.size();
    }
    action_features[action_id] = 1.0f;
    return action_features;
}

int AddiKulEnvLoader::getRotateAction(int action_id, utils::Rotation rotation) const
{
    if (action_id == kAddiKulPassActionID) { return action_id; }
    int from = action_id / kAddiKulBoardArea;
    int dest = action_id % kAddiKulBoardArea;
    int rotated_from = getRotatePosition(from, rotation);
    int rotated_dest = getRotatePosition(dest, rotation);
    return rotated_from * kAddiKulBoardArea + rotated_dest;
}

} // namespace minizero::env::addikul
