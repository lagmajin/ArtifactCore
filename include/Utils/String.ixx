module;
#include <Qstring>
#include <QVector>
#include <qminmax.h>

export module Utils.String;

import std;

export namespace ArtifactCore {

 int calculateLevenshteinDistance(const QString& s1, const QString& s2) {
  const int len1 = s1.length();
  const int len2 = s2.length();

  if (len1 == 0) {
   return len2;
  }
  if (len2 == 0) {
   return len1;
  }

  QVector<QVector<int>> d(len1 + 1, QVector<int>(len2 + 1));

  for (int i = 0; i <= len1; ++i) {
   d[i][0] = i;
  }
  for (int j = 0; j <= len2; ++j) {
   d[0][j] = j;
  }

  for (int i = 1; i <= len1; ++i) {
   for (int j = 1; j <= len2; ++j) {
	int cost = (s1.at(i - 1) == s2.at(j - 1)) ? 0 : 1;

	// ここを std::min を使って修正
	d[i][j] = std::min({
		d[i - 1][j] + 1,      // 削除
		d[i][j - 1] + 1,      // 挿入
		d[i - 1][j - 1] + cost // 置換
	 });
   }
  }

  return d[len1][len2];
 }

};