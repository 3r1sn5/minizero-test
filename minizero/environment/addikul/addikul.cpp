#include "addikul.h"
#include "sgf_loader.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cmath>
#include <sstream>

namespace minizero::env::addikul {

namespace {
constexpr std::array<std::pair<int, int>, 5> kP1StepDirs = {{{0, 1}, {-1, 1}, {1, 1}, {-1, 0}, {1, 0}}};
constexpr std::array<std::pair<int, int>, 5> kP2StepDirs = {{{0, -1}, {-1, -1}, {1, -1}, {-1, 0}, {1, 0}}};
constexpr std::array<std::pair<int, int>, 8> kP1CaptureDirs = {{{0, 1}, {-1, 1}, {1, 1}, {-1, 0}, {1, 0}, {0, -1}, {-1, -1}, {1, -1}}};
constexpr std::array<std::pair<int, int>, 8> kP2CaptureDirs = {{{0, -1}, {-1, -1}, {1, -1}, {-1, 0}, {1, 0}, {0, 1}, {-1, 1}, {1, 1}}};

inline bool isInBoardFast(int x, int y, int board_size)
{
    return (static_cast<unsigned>(x) < static_cast<unsigned>(board_size) &&
            static_cast<unsigned>(y) < static_cast<unsigned>(board_size));
}
} // namespace

AddiKulAction::AddiKulAction(const std::vector<std::string>& action_string_args, int board_size)
{
    assert(!action_string_args.empty());
    std::string player_token = action_string_args[0];
    std::string player_lower;
    player_lower.reserve(player_token.size());
    for (char c : player_token) { player_lower.push_back(static_cast<char>(std::tolower(c))); }
    if (player_token.size() == 1) {
        player_ = charToPlayer(player_token[0]);
    } else if (player_lower == "black") {
        player_ = Player::kPlayer1;
    } else if (player_lower == "white") {
        player_ = Player::kPlayer2;
    } else {
        player_ = Player::kPlayerSize;
    }
    assert(static_cast<int>(player_) > 0 && static_cast<int>(player_) <= kAddiKulNumPlayer);
    board_size_ = board_size;
    assert(action_string_args.size() >= 2);
    std::string move;
    for (size_t i = 1; i < action_string_args.size(); ++i) { move += action_string_args[i]; }
    move = normalizeMoveString(move);
    const auto [from_str, dest_str] = splitMoveString(move);
    int from = parsePosition(from_str, board_size);
    int dest = parsePosition(dest_str, board_size);
    action_id_ = from * board_size * board_size + dest;
}

std::string AddiKulAction::toConsoleString() const
{
    int from = getFromID(board_size_);
    int dest = getDestID(board_size_);
    return minizero::utils::SGFLoader::actionIDToBoardCoordinateString(from, board_size_) +
           minizero::utils::SGFLoader::actionIDToBoardCoordinateString(dest, board_size_);
}

int AddiKulAction::parsePosition(const std::string& coord, int board_size) const
{
    int pos = minizero::utils::SGFLoader::boardCoordinateStringToActionID(coord, board_size);
    assert(pos >= 0 && pos < board_size * board_size);
    return pos;
}

std::string AddiKulAction::normalizeMoveString(const std::string& move) const
{
    std::string normalized;
    normalized.reserve(move.size());
    for (char c : move) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            normalized.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return normalized;
}

std::pair<std::string, std::string> AddiKulAction::splitMoveString(const std::string& move) const
{
    assert(!move.empty());
    size_t split = 1;
    while (split < move.size() && std::isdigit(static_cast<unsigned char>(move[split]))) { ++split; }
    assert(split < move.size());
    return {move.substr(0, split), move.substr(split)};
}

void AddiKulEnv::reset()
{
    turn_ = Player::kPlayer1;
    actions_.clear();
    observations_.clear();
    board_.assign(getBoardSize() * getBoardSize(), Player::kPlayerNone);
    captured_.reset();
    move_count = 0;

    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < getBoardSize(); ++col) {
            board_[row * getBoardSize() + col] = Player::kPlayer1;
        }
    }

    for (int row = getBoardSize() - 3; row < getBoardSize(); ++row) {
        for (int col = 0; col < getBoardSize(); ++col) {
            board_[row * getBoardSize() + col] = Player::kPlayer2;
        }
    }
}

bool AddiKulEnv::act(const AddiKulAction& action)
{
    if (!isLegalAction(action)) { return false; }
    actions_.push_back(action);

    int from = action.getFromID(getBoardSize());
    int dest = action.getDestID(getBoardSize());
    MoveInfo info;
    isCaptureMove(from, dest, action.getPlayer(), info);

    board_[from] = Player::kPlayerNone;
    board_[dest] = action.getPlayer();

    if (info.is_capture) {
        board_[info.jumped_pos] = Player::kPlayerNone;
        captured_.set(action.getPlayer(), captured_.get(action.getPlayer()) + 1);
    }

    ++move_count;
    turn_ = action.nextPlayer();
    return true;
}

bool AddiKulEnv::act(const std::vector<std::string>& action_string_args)
{
    if (action_string_args.empty()) { return false; }

    std::string first = action_string_args[0];
    std::string first_lower;
    first_lower.reserve(first.size());
    for (char c : first) { first_lower.push_back(static_cast<char>(std::tolower(c))); }
    bool has_color = (first.size() == 1 && (first[0] == 'B' || first[0] == 'b' || first[0] == 'W' || first[0] == 'w')) ||
                     (first_lower == "black" || first_lower == "white");

    std::vector<std::string> normalized_args;
    if (has_color) {
        normalized_args = action_string_args;
    } else {
        std::string move;
        for (const auto& token : action_string_args) { move += token; }
        normalized_args = {std::string(1, playerToChar(turn_)), move};
    }

    const Player original_turn = turn_;
    AddiKulAction action(normalized_args, getBoardSize());
    setTurn(action.getPlayer());
    if (act(action)) { return true; }
    setTurn(original_turn);
    return false;
}

std::vector<AddiKulAction> AddiKulEnv::getLegalActions() const
{
    std::vector<AddiKulAction> actions;
    const int board_size = getBoardSize();
    const int spatial = board_size * board_size;
    const Player opponent = getNextPlayer(turn_, kAddiKulNumPlayer);
    actions.reserve(kAddiKulPiecesPerPlayer * 13);

    const auto& step_dirs = (turn_ == Player::kPlayer1 ? kP1StepDirs : kP2StepDirs);
    const auto& capture_dirs = (turn_ == Player::kPlayer1 ? kP1CaptureDirs : kP2CaptureDirs);

    for (int pos = 0; pos < spatial; ++pos) {
        if (board_[pos] != turn_) { continue; }
        int x = pos % board_size;
        int y = pos / board_size;

        for (const auto& dir : step_dirs) {
            int nx = x + dir.first;
            int ny = y + dir.second;
            if (!isInBoardFast(nx, ny, board_size)) { continue; }
            int dest = ny * board_size + nx;
            if (board_[dest] != Player::kPlayerNone) { continue; }
            int action_id = pos * spatial + dest;
            actions.emplace_back(action_id, turn_);
        }

        for (const auto& dir : capture_dirs) {
            int nx = x + dir.first;
            int ny = y + dir.second;
            int jx = x + dir.first * 2;
            int jy = y + dir.second * 2;
            if (!isInBoardFast(nx, ny, board_size) || !isInBoardFast(jx, jy, board_size)) { continue; }
            int between = ny * board_size + nx;
            int dest = jy * board_size + jx;
            if (board_[between] != opponent) { continue; }
            if (board_[dest] != Player::kPlayerNone) { continue; }
            int action_id = pos * spatial + dest;
            actions.emplace_back(action_id, turn_);
        }
    }

    return actions;
}

bool AddiKulEnv::isLegalAction(const AddiKulAction& action) const
{
    const int board_size = getBoardSize();
    const int spatial = board_size * board_size;
    const Player player = action.getPlayer();
    if (player != Player::kPlayer1 && player != Player::kPlayer2) { return false; }
    if (player != turn_) { return false; }
    if (action.getActionID() < 0 || action.getActionID() >= spatial * spatial) { return false; }
    const int from = action.getFromID(board_size);
    const int dest = action.getDestID(board_size);
    if (from == dest) { return false; }
    if (board_[from] != player) { return false; }
    if (board_[dest] != Player::kPlayerNone) { return false; }

    MoveInfo info;
    const bool is_capture = isCaptureMove(from, dest, player, info);
    if (is_capture) { return true; }
    return isSimpleMove(from, dest, player);
}

bool AddiKulEnv::isTerminal() const
{
    if (captured_.get(Player::kPlayer1) >= kAddiKulPiecesPerPlayer) { return true; }
    if (captured_.get(Player::kPlayer2) >= kAddiKulPiecesPerPlayer) { return true; }
    if (move_count >= kAddiKulMaxMoves) { return true; }
    return !hasAnyLegalAction();
}

float AddiKulEnv::getEvalScore(bool is_resign) const
{
    Player result = (is_resign ? getNextPlayer(turn_, kAddiKulNumPlayer) : evalWinner());
    switch (result) {
        case Player::kPlayer1: return 1.0f;
        case Player::kPlayer2: return -1.0f;
        default: return 0.0f;
    }
}

std::vector<float> AddiKulEnv::getFeatures(utils::Rotation rotation) const
{
    const int spatial = getBoardSize() * getBoardSize();
    const Player opponent = getNextPlayer(turn_, kAddiKulNumPlayer);
    const float is_p1_turn = (turn_ == Player::kPlayer1 ? 1.0f : 0.0f);
    const float is_p2_turn = (turn_ == Player::kPlayer2 ? 1.0f : 0.0f);

    std::vector<float> features(4 * spatial, 0.0f);
    for (int pos = 0; pos < spatial; ++pos) {
        const int rotation_pos = getRotatePosition(pos, utils::reversed_rotation[static_cast<int>(rotation)]);
        features[pos] = (board_[rotation_pos] == turn_ ? 1.0f : 0.0f);
        features[spatial + pos] = (board_[rotation_pos] == opponent ? 1.0f : 0.0f);
        features[2 * spatial + pos] = is_p1_turn;
        features[3 * spatial + pos] = is_p2_turn;
    }
    return features;
}

std::vector<float> AddiKulEnv::getActionFeatures(const AddiKulAction& action, utils::Rotation rotation) const
{
    const int board_size = getBoardSize();
    const int spatial = board_size * board_size;
    std::vector<float> action_features(2 * spatial, 0.0f);
    int from = getRotatePosition(action.getFromID(board_size), rotation);
    int dest = getRotatePosition(action.getDestID(board_size), rotation);
    action_features[from] = 1.0f;
    action_features[spatial + dest] = 1.0f;
    return action_features;
}

std::string AddiKulEnv::toString() const
{
    std::ostringstream oss;
    oss << getCoordinateString() << std::endl;
    for (int row = getBoardSize() - 1; row >= 0; --row) {
        oss << (row + 1) << " ";
        for (int col = 0; col < getBoardSize(); ++col) {
            Player p = board_[row * getBoardSize() + col];
            if (p == Player::kPlayerNone) {
                oss << " . ";
            } else if (p == Player::kPlayer1) {
                oss << " O ";
            } else {
                oss << " X ";
            }
        }
        oss << " " << (row + 1) << std::endl;
    }
    oss << getCoordinateString() << std::endl;
    return oss.str();
}

int AddiKulEnv::getRotateAction(int action_id, utils::Rotation rotation) const
{
    int spatial = getBoardSize() * getBoardSize();
    int from = action_id / spatial;
    int dest = action_id % spatial;
    int rot_from = getRotatePosition(from, rotation);
    int rot_dest = getRotatePosition(dest, rotation);
    return rot_from * spatial + rot_dest;
}

Player AddiKulEnv::evalWinner() const
{
    if (captured_.get(Player::kPlayer1) >= kAddiKulPiecesPerPlayer) { return Player::kPlayer1; }
    if (captured_.get(Player::kPlayer2) >= kAddiKulPiecesPerPlayer) { return Player::kPlayer2; }
    if (move_count >= kAddiKulMaxMoves) {
        if (!config::env_addikul_use_capture_tiebreak) { return Player::kPlayerNone; }
        int player1_captures = captured_.get(Player::kPlayer1);
        int player2_captures = captured_.get(Player::kPlayer2);
        if (player1_captures > player2_captures) { return Player::kPlayer1; }
        if (player2_captures > player1_captures) { return Player::kPlayer2; }
        return Player::kPlayerNone;
    }

    return Player::kPlayerNone;
}

Player AddiKulEnv::getPlayerAtBoardPos(int pos) const
{
    if (pos < 0 || pos >= getBoardSize() * getBoardSize()) { return Player::kPlayerNone; }
    return board_[pos];
}

bool AddiKulEnv::hasAnyLegalAction() const
{
    const int board_size = getBoardSize();
    const int spatial = board_size * board_size;
    const Player opponent = getNextPlayer(turn_, kAddiKulNumPlayer);

    const auto& step_dirs = (turn_ == Player::kPlayer1 ? kP1StepDirs : kP2StepDirs);
    const auto& capture_dirs = (turn_ == Player::kPlayer1 ? kP1CaptureDirs : kP2CaptureDirs);

    for (int pos = 0; pos < spatial; ++pos) {
        if (board_[pos] != turn_) { continue; }
        const int x = pos % board_size;
        const int y = pos / board_size;

        for (const auto& dir : step_dirs) {
            const int nx = x + dir.first;
            const int ny = y + dir.second;
            if (!isInBoardFast(nx, ny, board_size)) { continue; }
            if (board_[ny * board_size + nx] == Player::kPlayerNone) { return true; }
        }

        for (const auto& dir : capture_dirs) {
            const int nx = x + dir.first;
            const int ny = y + dir.second;
            const int jx = x + dir.first * 2;
            const int jy = y + dir.second * 2;
            if (!isInBoardFast(nx, ny, board_size) || !isInBoardFast(jx, jy, board_size)) { continue; }
            const int between = ny * board_size + nx;
            const int dest = jy * board_size + jx;
            if (board_[between] == opponent && board_[dest] == Player::kPlayerNone) { return true; }
        }
    }

    return false;
}

bool AddiKulEnv::isCaptureMove(int from, int dest, Player player, MoveInfo& info) const
{
    const int board_size = getBoardSize();
    const int fx = from % board_size;
    const int fy = from / board_size;
    const int dx = dest % board_size;
    const int dy = dest / board_size;
    const int delta_x = dx - fx;
    const int delta_y = dy - fy;
    if (!((delta_x == 0 && std::abs(delta_y) == 2) ||
          (std::abs(delta_x) == 2 && std::abs(delta_y) == 2) ||
          (std::abs(delta_x) == 2 && delta_y == 0))) {
        return false;
    }

    const int mid_x = fx + delta_x / 2;
    const int mid_y = fy + delta_y / 2;
    const int mid = mid_y * board_size + mid_x;
    if (board_[mid] != getNextPlayer(player, kAddiKulNumPlayer)) { return false; }
    info.is_capture = true;
    info.jumped_pos = mid;
    return true;
}

bool AddiKulEnv::isSimpleMove(int from, int dest, Player player) const
{
    const int board_size = getBoardSize();
    const int fx = from % board_size;
    const int fy = from / board_size;
    const int dx = dest % board_size;
    const int dy = dest / board_size;
    const int delta_x = dx - fx;
    const int delta_y = dy - fy;

    if (player == Player::kPlayer1) {
        if ((delta_x == 0 && delta_y == 1) || (std::abs(delta_x) == 1 && delta_y == 1)) { return true; }
    } else {
        if ((delta_x == 0 && delta_y == -1) || (std::abs(delta_x) == 1 && delta_y == -1)) { return true; }
    }

    if ((std::abs(delta_x) == 1) && delta_y == 0) { return true; }
    return false;
}

std::string AddiKulEnv::getCoordinateString() const
{
    std::ostringstream oss;
    oss << "  ";
    for (int col = 0; col < getBoardSize(); ++col) {
        oss << " " << static_cast<char>('A' + col) << " ";
    }
    return oss.str();
}

std::vector<float> AddiKulEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation) const
{
    const AddiKulAction& action = action_pairs_[pos].first;
    int spatial = getBoardSize() * getBoardSize();
    std::vector<float> action_features(2 * spatial, 0.0f);
    int action_id = ((pos < static_cast<int>(action_pairs_.size())) ? getRotateAction(action.getActionID(), rotation) : 0);
    int from = action_id / spatial;
    int dest = action_id % spatial;
    action_features[from] = 1.0f;
    action_features[spatial + dest] = 1.0f;
    return action_features;
}

int AddiKulEnvLoader::getRotateAction(int action_id, utils::Rotation rotation) const
{
    int spatial = getBoardSize() * getBoardSize();
    int from = action_id / spatial;
    int dest = action_id % spatial;
    int rot_from = getRotatePosition(from, rotation);
    int rot_dest = getRotatePosition(dest, rotation);
    return rot_from * spatial + rot_dest;
}

} // namespace minizero::env::addikul
