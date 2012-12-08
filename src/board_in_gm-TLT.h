#ifndef BOARD_IN_GM_TLT_H
#define BOARD_IN_GM_TLT_H

#include "board_in_gm.h"

#include <cstring>

#include <bitset>
#include <vector>
#include <algorithm>
#include <functional>

#include "airset_util.h"

using namespace std;


inline static PlayerColor OppstColor(PlayerColor color)
{
    return ~color & 1;
}


template <BoardLen BOARD_LEN>
BoardInGm<BOARD_LEN>::BoardInGm() {}


template <BoardLen BOARD_LEN>
void BoardInGm<BOARD_LEN>::Init()
{
    board_.Init();
//    for (int i=0; i<2; ++i) chain_sets_[i].Init();
    for (int i=0; i<2; ++i) playable_indxs_[i].Init();
}


template <BoardLen BOARD_LEN>
void BoardInGm<BOARD_LEN>::Copy(const BoardInGm &b)
{
    board_.Copy(b.board_);
    for (int i=0; i<2; ++i) chain_sets_[i].Copy(b.chain_sets_[i]);
    for (int i=0; i<2; ++i) playable_indxs_[i].Copy(b.playable_indxs_[i]);
}


template <BoardLen BOARD_LEN>
bool BoardInGm<BOARD_LEN>::PlayMove(const Move &move)
{
    PlayerColor color = move.color_;
    PointIndex indx = move.indx_;
    FOO_ASSERT(board_.GetPoint(indx) == EMPTY_POINT);

    board_.SetPoint(indx, color);
    for (int i=0; i<2; ++i) playable_indxs_[i].Remove(indx);
    for (int i=0; i<2; ++i) chain_sets_[i].LetAdjcntChainsSetAir(indx, false);

    PlayerColor oc = OppstColor(color);
    bitset<BoardLenSquare<BOARD_LEN>()> air_set;
    auto &ins = this->GetPosClcltr();
    const Position &pos = ins.GetPos(indx);

    for (int i=0; i<4; ++i) {
        Position adj_pos = pos.AdjcntPos(i);
        if (!ins.IsInBoard(adj_pos)) continue;
        PointIndex adj_indx = ins.GetIndex(adj_pos);

        if (board_.GetPoint(adj_pos) == oc &&
                chain_sets_[(int)oc].GetAirCountByPiece(adj_indx) == 0) {
            this->RemoveChain(Move(oc, adj_indx));
        }
        if (board_.GetPoint(adj_pos) == EMPTY_POINT) {
            SetAir<BOARD_LEN>(&air_set, adj_pos);
        }
    }

    chain_sets_[(int)color].AddPiece(indx, air_set);
    return chain_sets_[(int)color].GetAirCountByPiece(indx) == 0 ?
        false : true;
}


template <BoardLen BOARD_LEN>
PosCalculator<BOARD_LEN> &BoardInGm<BOARD_LEN>::GetPosClcltr() const
{
    return board_.GetPosClcltr();
}


template <BoardLen BOARD_LEN>
bool BoardInGm<BOARD_LEN>::IsEye(const Move &move) const
{
    FOO_ASSERT(board_.GetPoint(move.indx_) == EMPTY_POINT);
    auto &ins = this->GetPosClcltr();
    const Position &pos = ins.GetPos(move.indx_);
    PlayerColor color = move.color_;

    for (int i=0; i<4; ++i) {
        const Position &adj_pos = pos.AdjcntPos(i);
        if (!ins.IsInBoard(adj_pos)) {
            continue;
        } else if (board_.GetPoint(pos.AdjcntPos(i)) != color) {
            return false;
        }
    }

    return true;
}


template <BoardLen BOARD_LEN>
bool BoardInGm<BOARD_LEN>::IsRealEye(const Move &move) const
{
    FOO_ASSERT(board_.GetPoint(move.indx_) == EMPTY_POINT);
    if (!this->IsEye(move)) return false;
    const Position &pos = this->GetPosClcltr().GetPos(move.indx_);
    PlayerColor color = move.color_;
    auto &ins = this->GetPosClcltr();
    auto status = ins.GetPosStatus(pos);
    PointIndex count = 0;

    for (int i=0; i<4; ++i) {
        Position oblq_pos = pos.ObliquePos(i);
        if (!ins.IsInBoard(oblq_pos)) continue;

        count = (board_.GetPoint(oblq_pos) == color) ? count + 1 : count;
    }

    static const PointIndex TABLE[3] = {3, 2, 1};
    return TABLE[status] == count;
}


template <BoardLen BOARD_LEN>
inline bool BoardInGm<BOARD_LEN>::IsFakeEye(const Move &move) const
{
    return this->IsEye(move) && !this->IsRealEye(move);
}


template <BoardLen BOARD_LEN>
bool BoardInGm<BOARD_LEN>::IsEmptySingly(PointIndex indx) const
{
    auto &ins = this->GetPosClcltr();
    const Position &pos = ins.GetPos(indx);

    for (int i=0; i<4; ++i) {
        Position adj_pos = pos.AdjcntPos(i);
        if (!ins.IsInBoard(adj_pos)) continue;

        if (board_.GetPoint(adj_pos) == EMPTY_POINT) return false;
    }

    return true;
}


template <BoardLen BOARD_LEN>
bool BoardInGm<BOARD_LEN>::IsMoveSuiside(const Move &move) const
{
    FOO_ASSERT(board_.GetPoint(move.indx_) == EMPTY_POINT);
    BoardInGm<BOARD_LEN> copy;
    copy.Copy(*this);
    return !copy.PlayMove(move);
}


template <BoardLen BOARD_LEN>
void BoardInGm<BOARD_LEN>::RemoveChain(const Move &move)
{
    auto p_chnset = chain_sets_ + move.color_;
    auto trp = p_chnset->GetPieces(move.indx_);
    chain_sets_[(int)move.color_].RemoveListByPiece(move.indx_);
    PlayerColor oc = OppstColor(move.color_);

    if (trp.size() > 0) {
        for (PointIndex indx: trp) {
            for (int i=0; i<2; ++i) playable_indxs_[i].Add(indx);
        }
    }

    for (PointIndex indx : trp) {
        board_.SetPoint(indx, EMPTY_POINT);
        chain_sets_[(int)oc].LetAdjcntChainsSetAir(indx, true);
    }
}


template <BoardLen BOARD_LEN>
void BoardInGm<BOARD_LEN>::UpdtAdjPlblIndxsOfChn(PointIndex indx)
{
    FOO_ASSERT(board_.GetPoint(indx) != EMPTY_POINT);
    PlayerColor color = board_.GetPoint(indx);
    const auto p_c = chain_sets_ + color;
    AirCount air_c = p_c->GetAirCountByPiece(indx);
    if (air_c == 1) {
        Move move(color, indx);
        PointIndex air_indx = (p_c->GetPieces(indx))[0];
        PlayerColor oc = OppstColor(color);
        if (this->IsRealEye(move)) {
            playable_indxs_[color].Remove(air_indx);
            playable_indxs_[oc].Add(air_indx);
        } else if (this->IsFakeEye(move)) {
            playable_indxs_[color].Set(air_indx, !this->IsMoveSuiside(move));
            playable_indxs_[oc].Add(air_indx);
        } else /* not eye */ {
            playable_indxs_[color].Set(air_indx, !this->IsMoveSuiside(move));
            playable_indxs_[oc].Add(air_indx);
        }
    } else {
    }
}


template <BoardLen BOARD_LEN>
void BoardInGm<BOARD_LEN>::UpdtPlblIndxsArnd(PointIndex indx)
{
    this->UpdtAdjPlblIndxsOfChn(indx);
    auto &ins = this->GetPosClcltr();
    Position pos = ins.GetPos(indx);
    PlayerColor color = board_.GetPoint(indx);
    PlayerColor oc = OppstColor(color);
    function<void(PointIndex)> Funs[3];

    Funs[(int)color] = [](PointIndex) {};
    Funs[(int)oc] = &BoardInGm<BOARD_LEN>::UpdtAdjPlblIndxsOfChn;
    Funs[(int)EMPTY_POINT] = [this](PointIndex indx) {
        if (!this->IsEmptySingly(indx)) return;

        for (int i=0; i<2; ++i) {
            if (this->IsMoveSuiside(Move(i, indx))) {
                playable_indxs_[i].Remove(indx);
            }
        }
    };

    for (int i=0; i<4; ++i) {
        Position adj_pos = pos.AdjcntPos(i);
        if (!ins.IsInBoard(adj_pos)) continue;

        Point point = board_.GetPoint(adj_pos);
        PointIndex adj_indx = ins.GetIndex(adj_pos);
        Funs[(int)point](adj_indx);
    }
}


#endif