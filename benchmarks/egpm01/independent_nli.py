"""Independent small NLI model. Threshold fixed before benchmark; neutral is not refutation."""
import torch
from transformers import AutoTokenizer,AutoModelForSequenceClassification
from huggingface_hub import model_info
MODEL="cross-encoder/nli-deberta-v3-small"
class Verifier:
    def __init__(self):
        torch.set_num_threads(2);self.revision=model_info(MODEL).sha
        self.tokenizer=AutoTokenizer.from_pretrained(MODEL,revision=self.revision)
        self.model=AutoModelForSequenceClassification.from_pretrained(MODEL,revision=self.revision).eval()
        self.labels={int(i):str(v).lower() for i,v in self.model.config.id2label.items()}
        if set(self.labels.values())!={"entailment","contradiction","neutral"}:raise ValueError("unknown NLI labels "+str(self.labels))
    def __call__(self,premise,hypothesis):
        x=self.tokenizer(premise,hypothesis,return_tensors="pt",truncation=True,max_length=512)
        with torch.inference_mode():p=self.model(**x).logits.softmax(-1)[0].tolist()
        scores={self.labels[i]:float(v) for i,v in enumerate(p)}
        v="support" if scores["entailment"]>=.8 else "refute" if scores["contradiction"]>=.8 else "unresolved"
        return v,scores
