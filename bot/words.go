package main

// chatWords 는 전체 채팅/친구 채팅에 쓰이는 단어 풀이다.
// 서버 입장에서 의미는 없고, 길이와 빈도만 흉내내면 되므로 짧은 단어로 채운다.
var chatWords = []string{
	"hello", "hi", "gg", "wp", "nice", "lol", "omg", "wow", "hmm", "ok",
	"go", "run", "boss", "raid", "party", "quest", "drop", "gold", "item", "sword",
	"enhance", "fail", "success", "lucky", "unlucky", "trade", "buy", "sell", "cheap", "expensive",
	"where", "here", "there", "wait", "hurry", "sorry", "thanks", "welcome", "bye", "afk",
	"server", "lag", "ping", "shard", "queue", "packet", "socket", "iocp", "redis", "postgres",
	"level", "up", "down", "power", "rank", "top", "loser", "winner", "again", "one",
	"more", "time", "today", "night", "morning", "raidboss", "dungeon", "guild", "friend", "invite",
}

// 봇이 만들어 내는 한 메시지의 단어 수 범위.
const (
	minChatWords = 1
	maxChatWords = 12
)
