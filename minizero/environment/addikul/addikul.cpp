#include "addikul.h"
#include "sgf_loader.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <sstream>
#include <utility>

namespace minizero::env::addikul {

using namespace minizero::utils;

AddiKulAction::AddiKulAction(const std::vector<std::string>& action_string_args, int board_size)
{
    assert(action_string_args.size() >= 2);
    player_ = Player::kPlayerNone;

    // Some callers omit the leading player token (passing only FROM/TO), and some legacy logs
    // encode it as unrecognized characters (e.g., "1"/"2"). Default to the current turn in both
    // cases instead of treating the move as illegal.
    int move_start = 0;
    if (action_string_args[0].size() == 1) {
        player_ = charToPlayer(action_string_args[0][0]);
        if (player_ != Player::kPlayerSize) { move_start = 1; }
    }
    if (player_ == Player::kPlayerSize) { player_ = Player::kPlayerNone; }

    std::vector<std::string> move_tokens;

    int remaining = static_cast<int>(action_string_args.size()) - move_start;
    if (remaining >= 2) {
        move_tokens.assign(action_string_args.begin() + move_start, action_string_args.begin() + move_start + 2);
    } 
    else {
        // Accept compact strings like "E3F4" or "E3-F4" that arrive as a single token in GTP.
        std::string packed = action_string_args.back();
        std::string token;
        for (char ch : packed) {
            if (std::isalpha(ch) || std::isdigit(ch)) {
                token.push_back(ch);
                if (std::isdigit(ch)) {
                    move_tokens.push_back(token);
                    token.clear();
                }
            } else if (!token.empty()) {
                move_tokens.push_back(token);
                token.clear();
            }
        }
        if (!token.empty()) { move_tokens.push_back(token); }
    }

    if (move_tokens.size() < 2) {
        action_id_ = -1;
        return;
    }

    assert(move_tokens.size() == 2);
    int from = SGFLoader::boardCoordinateStringToActionID(move_tokens[0], board_size);
    int dest = SGFLoader::boardCoordinateStringToActionID(move_tokens[1], board_size);
    action_id_ = encode(from, dest, board_size);
}

std::string AddiKulAction::toConsoleString() const
{
    if (action_id_ < 0 || player_ == Player::kPlayerNone) { return "pass"; }
    int board_size = kAddiKulBoardSize;
    std::string from = SGFLoader::actionIDToBoardCoordinateString(getFromID(board_size), board_size);
    std::string dest = SGFLoader::actionIDToBoardCoordinateString(getDestID(board_size), board_size);
    std::string act = from + dest;
    std::transform(act.begin(), act.end(), act.begin(), [](unsigned char c) {return std::tolower(c); });
    return act;
}

void AddiKulEnv::reset()
{
    turn_ = Player::kPlayer1;
    actions_.clear();
    board_.assign(kAddiKulBoardSize * kAddiKulBoardSize, Player::kPlayerNone);
    state_counts_.clear();

    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < kAddiKulBoardSize; ++col) {
            board_[row * kAddiKulBoardSize + col] = Player::kPlayer1;
        }
    }
    for (int row = kAddiKulBoardSize - 3; row < kAddiKulBoardSize; ++row) {
        for (int col = 0; col < kAddiKulBoardSize; ++col) {
            board_[row * kAddiKulBoardSize + col] = Player::kPlayer2;
        }
    }

    recordState();
}

bool AddiKulEnv::act(const AddiKulAction& action)
{
    if (action.getActionID() < 0) { return false; }
    AddiKulAction applied_action = action;
    if (!getLegalAppliedAction(action, applied_action)) { return false; }

    actions_.push_back(applied_action);
    int from = applied_action.getFromID(getBoardSize());
    int dest = applied_action.getDestID(getBoardSize());
    int from_row = from / getBoardSize();
    int from_col = from % getBoardSize();
    int dest_row = dest / getBoardSize();
    int dest_col = dest % getBoardSize();

    board_[from] = Player::kPlayerNone;
    board_[dest] = applied_action.getPlayer();

    int dr = dest_row - from_row;
    int dc = dest_col - from_col;
    if (std::abs(dr) == 2 || std::abs(dc) == 2) {
        int cap_row = from_row + dr / 2;
        int cap_col = from_col + dc / 2;
        board_[cap_row * getBoardSize() + cap_col] = Player::kPlayerNone;
    }

    turn_ = applied_action.nextPlayer();
    recordState();
    return true;
}

bool AddiKulEnv::act(const std::vector<std::string>& action_string_args)
{
    return act(AddiKulAction(action_string_args));
}

std::vector<AddiKulAction> AddiKulEnv::getLegalActions() const
{
    std::vector<AddiKulAction> actions;
    const int board_area = getBoardSize() * getBoardSize();

    auto enumerate_directions = [&](Player player) {
        std::vector<std::pair<int, int>> dirs;
        int forward = (player == Player::kPlayer1) ? 1 : -1;
        dirs.emplace_back(forward, 0);      // forward
        dirs.emplace_back(forward, -1);     // forward-left
        dirs.emplace_back(forward, 1);      // forward-right
        dirs.emplace_back(0, -1);           // left
        dirs.emplace_back(0, 1);            // right
        return dirs;
    };

    const auto directions = enumerate_directions(turn_);

    for (int pos = 0; pos < board_area; ++pos) {
        if (board_[pos] != turn_) { continue; }
        int row = pos / getBoardSize();
        int col = pos % getBoardSize();

        for (const auto& [dr, dc] : directions) {
            int step_row = row + dr;
            int step_col = col + dc;
            if (step_row >= 0 && step_row < getBoardSize() && step_col >= 0 && step_col < getBoardSize()) {
                int step_pos = step_row * getBoardSize() + step_col;
                if (board_[step_pos] == Player::kPlayerNone) {
                    actions.emplace_back(AddiKulAction(AddiKulAction::encode(pos, step_pos, getBoardSize()), turn_));
                }
            }

                int jump_row = row + dr * 2;
            int jump_col = col + dc * 2;
            if (jump_row < 0 || jump_row >= getBoardSize() || jump_col < 0 || jump_col >= getBoardSize()) { continue; }
            int middle_row = row + dr;
            int middle_col = col + dc;
            int middle_pos = middle_row * getBoardSize() + middle_col;
            int jump_pos = jump_row * getBoardSize() + jump_col;
            if (board_[middle_pos] == getNextPlayer(turn_, kAddiKulNumPlayer) && board_[jump_pos] == Player::kPlayerNone) {
                actions.emplace_back(AddiKulAction(AddiKulAction::encode(pos, jump_pos, getBoardSize()), turn_));
            }
        }
    }
    return actions;
}

bool AddiKulEnv::isLegalAction(const AddiKulAction& action) const
{
    AddiKulAction applied_action;
    return getLegalAppliedAction(action, applied_action);
}

bool AddiKulEnv::getLegalAppliedAction(const AddiKulAction& action, AddiKulAction& applied_action) const
{
    if (action.getActionID() < 0) { return false; }

    Player actor = (action.getPlayer() == Player::kPlayerNone) ? turn_ : action.getPlayer();
    if (actor != turn_) { return false; }

    // Primary path: treat the incoming action as already using board coordinates
    // (Player1 orientation). This matches how policy logits are interpreted for
    // other board games in MiniZero.
    AddiKulAction board_oriented_action(action.getActionID(), actor);
    if (isDirectLegalAction(board_oriented_action)) {
        applied_action = board_oriented_action;
        return true;
    }

    // Fallback: if the caller encoded Player2 moves from their own perspective
    // (180° rotated), rotate once into board coordinates for legality checks.
    if (turn_ == Player::kPlayer2) {
        AddiKulAction rotated_action(getRotateAction(action.getActionID(), utils::Rotation::kRotation180), turn_);
        if (isDirectLegalAction(rotated_action)) {
            applied_action = rotated_action;
            return true;
        }
    }
    return false;
}

bool AddiKulEnv::isDirectLegalAction(const AddiKulAction& action) const
{
    int board_area = getBoardSize() * getBoardSize();
    if (action.getActionID() < 0 || action.getActionID() >= board_area * board_area) { return false; }
    if (action.getPlayer() != turn_) { return false; }

    int from = action.getFromID(getBoardSize());
    int dest = action.getDestID(getBoardSize());
    if (from == dest) { return false; }
    if (from < 0 || from >= board_area || dest < 0 || dest >= board_area) { return false; }
    if (board_[from] != turn_ || board_[dest] != Player::kPlayerNone) { return false; }

    int from_row = from / getBoardSize();
    int from_col = from % getBoardSize();
    int dest_row = dest / getBoardSize();
    int dest_col = dest % getBoardSize();

    int dr = dest_row - from_row;
    int dc = dest_col - from_col;

    auto enumerate_directions = [&](Player player) {
        std::vector<std::pair<int, int>> dirs;
        int forward = (player == Player::kPlayer1) ? 1 : -1;
        dirs.emplace_back(forward, 0);      // forward
        dirs.emplace_back(forward, -1);     // forward-left
        dirs.emplace_back(forward, 1);      // forward-right
        dirs.emplace_back(0, -1);           // left
        dirs.emplace_back(0, 1);            // right
        return dirs;
    };

    const auto directions = enumerate_directions(turn_);

    auto is_step_direction = [&](int r, int c) {
        return std::any_of(directions.begin(), directions.end(), [&](const auto& dir) {
            return dir.first == r && dir.second == c;
        });
    };

    if (std::abs(dr) <= 1 && std::abs(dc) <= 1) { return is_step_direction(dr, dc); }

    if (std::abs(dr) == 2 || std::abs(dc) == 2) {
        if (!is_step_direction(dr / 2, dc / 2)) { return false; }
        int cap_row = from_row + dr / 2;
        int cap_col = from_col + dc / 2;
        int cap_pos = cap_row * getBoardSize() + cap_col;
        return board_[cap_pos] == getNextPlayer(turn_, kAddiKulNumPlayer);
    }

    return false;
}

Player AddiKulEnv::eval() const
{
    // Piece-elimination victory: if a player has no pieces left, they lose.
    int player1_count = std::count(board_.begin(), board_.end(), Player::kPlayer1);
    int player2_count = std::count(board_.begin(), board_.end(), Player::kPlayer2);
    if (player1_count == 0 && player2_count == 0) { return Player::kPlayerNone; }
    if (player1_count == 0) { return Player::kPlayer2; }
    if (player2_count == 0) { return Player::kPlayer1; }

    // Repetition rule: if the same state occurs 3 times, decide the winner by captures.
    // Each player starts with 21 pieces (3 rows * 7 cols).
    if (isDraw()) {
        const int kPiecesPerPlayer = 21;
        int p1_captures = kPiecesPerPlayer - player2_count; // Player 1 captures Player 2 pieces
        int p2_captures = kPiecesPerPlayer - player1_count; // Player 2 captures Player 1 pieces
        if (p1_captures > p2_captures) { return Player::kPlayer1; }
        if (p2_captures > p1_captures) { return Player::kPlayer2; }
        return Player::kPlayerNone; // equal captures => draw
    }

    auto legal = getLegalActions();
    if (legal.empty()) { return getNextPlayer(turn_, kAddiKulNumPlayer); }

    return Player::kPlayerNone;
}

bool AddiKulEnv::isTerminal() const
{
    if (eval() != Player::kPlayerNone) { return true; }
    return isDraw();
}

float AddiKulEnv::getEvalScore(bool is_resign /*= false*/) const
{
    Player winner = is_resign ? getNextPlayer(turn_, kAddiKulNumPlayer) : eval();
    switch (winner) {
        case Player::kPlayer1: return 1.0f;
        case Player::kPlayer2: return -1.0f;
        default: return 0.0f;
    }
}

std::vector<float> AddiKulEnv::getFeatures(utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    std::vector<float> features;
    for (int channel = 0; channel < getNumInputChannels(); ++channel) {
        for (int pos = 0; pos < getBoardSize() * getBoardSize(); ++pos) {
            int rotation_pos = getRotatePosition(pos, utils::reversed_rotation[static_cast<int>(rotation)]);
            if (channel == 0) {
                features.push_back(board_[rotation_pos] == turn_ ? 1.0f : 0.0f);
            } else if (channel == 1) {
                features.push_back(board_[rotation_pos] == getNextPlayer(turn_, kAddiKulNumPlayer) ? 1.0f : 0.0f);
            } else if (channel == 2) {
                features.push_back(turn_ == Player::kPlayer1 ? 1.0f : 0.0f);
            } else if (channel == 3) {
                features.push_back(turn_ == Player::kPlayer2 ? 1.0f : 0.0f);
            }
        }
    }
    return features;
}

std::vector<float> AddiKulEnv::getActionFeatures(const AddiKulAction& action, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    const int board_area = getBoardSize() * getBoardSize();
    std::vector<float> action_features(getNumActionFeatureChannels() * board_area, 0.0f);

    int rotated_action_id = getRotateAction(action.getActionID(), rotation);
    int rotated_from = rotated_action_id / board_area;
    int rotated_dest = rotated_action_id % board_area;
    if (rotated_from >= 0 && rotated_from < board_area && rotated_dest >= 0 && rotated_dest < board_area) {
        action_features[rotated_from] = 1.0f;
        action_features[board_area + rotated_dest] = 1.0f;
    }
    return action_features;
}

std::string AddiKulEnv::toString() const
{
    std::ostringstream oss;
    oss << "   A  B  C  D  E  F  G" << std::endl;
    for (int row = getBoardSize() - 1; row >= 0; --row) {
        oss << row + 1 << " ";
        for (int col = 0; col < getBoardSize(); ++col) {
            Player p = board_[row * getBoardSize() + col];
            if (p == Player::kPlayer1) {
                oss << " O ";
            } else if (p == Player::kPlayer2) {
                oss << " X ";
            } else {
                oss << " . ";
            }
        }
        oss << " " << row + 1 << std::endl;
    }
    oss << "   A  B  C  D  E  F  G" << std::endl;
    return oss.str();
}

int AddiKulEnv::getRotateAction(int action_id, utils::Rotation rotation) const
{
    int from = action_id / (getBoardSize() * getBoardSize());
    int dest = action_id % (getBoardSize() * getBoardSize());
    int rotated_from = getRotatePosition(from, rotation);
    int rotated_dest = getRotatePosition(dest, rotation);
    return AddiKulAction::encode(rotated_from, rotated_dest, getBoardSize());
}

std::string AddiKulEnv::getStateKey() const
{
    std::string key;
    key.reserve(board_.size() + 1);
    key.push_back(static_cast<char>(turn_));
    for (Player p : board_) {
        key.push_back(static_cast<char>(p));
    }
    return key;
}

void AddiKulEnv::recordState()
{
    std::string key = getStateKey();
    ++state_counts_[key];
}

bool AddiKulEnv::isDraw() const
{
    // Note: In Addi Kul, "draw" is triggered by 3-fold repetition.

    auto state_it = state_counts_.find(getStateKey());
    bool repeated_state = (state_it != state_counts_.end() && state_it->second >= 3);
    return repeated_state;
}

std::vector<float> AddiKulEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    const int board_size = getBoardSize();
    const int board_area = board_size * board_size;
    const int action_feature_size = 2 * board_area;
    std::vector<float> action_features(action_feature_size, 0.0f);

    if (pos < static_cast<int>(action_pairs_.size())) {
        const AddiKulAction& action = action_pairs_[pos].first;
        int rotated_action_id = getRotateAction(action.getActionID(), rotation);
        int rotated_from = rotated_action_id / board_area;
        int rotated_dest = rotated_action_id % board_area;
        if (rotated_from >= 0 && rotated_from < board_area && rotated_dest >= 0 && rotated_dest < board_area) {
            action_features[rotated_from] = 1.0f;
            action_features[board_area + rotated_dest] = 1.0f;
        }
    }
    return action_features;
}

int AddiKulEnvLoader::getRotateAction(int action_id, utils::Rotation rotation) const
{
    int board_size = getBoardSize();
    int from = action_id / (board_size * board_size);
    int dest = action_id % (board_size * board_size);
    int rotated_from = getRotatePosition(from, rotation);
    int rotated_dest = getRotatePosition(dest, rotation);
    return AddiKulAction::encode(rotated_from, rotated_dest, board_size);
}

} // namespace minizero::env::addikul
