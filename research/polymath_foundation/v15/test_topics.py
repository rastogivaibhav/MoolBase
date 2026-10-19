#!/usr/bin/env python3
"""F15 frozen-topic, withheld-answer and source-authenticity adversarial checks."""
import copy,json,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from prepare_topics import generate
class TopicTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src=Path(__file__).with_name("topics.json")
        cls.data=json.loads(cls.src.read_text())
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        self.docs=copy.deepcopy(self.data)
    def run_gen(self):
        file=self.root/"topics.json";file.write_text(json.dumps(self.docs))
        return generate(file,self.root/"out")
    def test_01_exact_original_20_long_documents(self):
        self.run_gen()
        c0=(self.root/"out"/"C0.tsv").read_text()
        c1=(self.root/"out"/"C1.tsv").read_text()
        self.assertEqual(c0.count("\n"),20)
        self.assertEqual(c1.count("\n"),21)
        self.assertNotIn("Garuda Indonesia is the national flag carrier airline of Indonesia",c0)
        self.assertIn("Garuda Indonesia is the national flag carrier airline of Indonesia",c1)
    def test_02_twenty_exact_ids(self):
        self.assertEqual([x["topic_id"] for x in self.docs],[f"T{i:02}" for i in range(1,21)])
    def test_03_minimum_substantive_words(self):
        self.assertGreaterEqual(min(len(x["body"].split()) for x in self.docs),125)
    def test_04_duplicate_id_rejected(self):
        self.docs[3]["topic_id"]="T01"
        with self.assertRaises(ValueError):self.run_gen()
    def test_05_direct_fact_in_two_documents_rejected(self):
        self.docs[0]["body"]+=" Garuda Indonesia is the national flag carrier airline of Indonesia."
        with self.assertRaises(ValueError):self.run_gen()
    def test_06_claimed_external_authentication_rejected(self):
        self.docs[3]["source_status"]="externally_verified"
        with self.assertRaises(ValueError):self.run_gen()
    def test_07_marked_answer_card_cannot_change(self):
        self.docs[4]["answer_bearing_identity_doc"]=True
        with self.assertRaises(ValueError):self.run_gen()
    def test_08_only_nineteen_articles_rejected(self):
        self.docs.pop()
        with self.assertRaises(ValueError):self.run_gen()
    def test_09_document_truncated_rejected(self):
        self.docs[7]["body"]="An airline."
        with self.assertRaises(ValueError):self.run_gen()
    def test_10_frozen_typo_question(self):
        self.run_gen()
        q=(self.root/"out"/"queries.tsv").read_text()
        self.assertIn("gradua airline",q)
        self.assertEqual(q.count("\n"),5)
    def test_11_evaluator_gold_not_in_native_source_tsv(self):
        self.run_gen()
        t=(self.root/"out"/"C1.tsv").read_text()
        self.assertNotIn("answer_bearing_identity_doc",t)
        self.assertNotIn("expected_answer_country_for_evaluator_only",t)
    def test_12_document_links_not_prefabricated(self):
        self.run_gen()
        t=(self.root/"out"/"C1.tsv").read_text()
        self.assertNotIn("source_family_id",t)
        self.assertNotIn("supports\tH-",t)
if __name__=="__main__":
    unittest.main(verbosity=2)
